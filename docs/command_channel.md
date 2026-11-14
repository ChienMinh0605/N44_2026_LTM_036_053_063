# Kênh lệnh điều khiển drone qua MQTT

Mở rộng đề tài: ngoài thu thập telemetry (chiều lên), hệ thống nhận **lệnh điều khiển** (chiều xuống)
qua cùng Broker. Điểm chính không phải "gửi được lệnh", mà là **các tính năng MQTT có lợi cho telemetry
lại nguy hiểm với lệnh**, nên Gateway phải kiểm tra từng lệnh trước khi chuyển cho drone.

```
Người điều khiển (App / commander) ──drone/001/cmd──▶ Broker ──▶ Gateway ──▶ drone (giả lập hoặc PX4)
                ▲                                                   │
                └────────────── drone/001/cmd/ack ──────────────────┘
```

## Topic

| Topic | Chiều | QoS | Retained |
|---|---|---|---|
| `drone/001/cmd` | người điều khiển → Gateway | 0 hoặc 1 (người gửi chọn) | **không bao giờ** |
| `drone/001/cmd/ack` | Gateway → người điều khiển | 1 | không |

## Lệnh (JSON gửi lên `drone/001/cmd`)

```json
{ "cmd_id": 1794700000123, "from": "op1", "action": "goto",
  "lat": 47.3980, "lon": 8.5460, "alt": 20,
  "sent_ms": 1794700000123, "ttl_ms": 3000 }
```

| Trường | Bắt buộc | Ý nghĩa |
|---|---|---|
| `cmd_id` | có | số định danh lệnh, khác nhau giữa các lệnh của cùng một người gửi |
| `from` | có | tên người điều khiển (ví dụ `op1`, `app-tien`) |
| `action` | có | `takeoff`, `land`, `rtl` (quay về và hạ cánh), `goto`, `hold` (đứng yên), `release` (nhả quyền) |
| `sent_ms` | có | Unix ms lúc gửi, theo đồng hồ máy người gửi |
| `ttl_ms` | không | hạn dùng, mặc định 3000 ms |
| `alt` | không | mét so với điểm cất cánh (`takeoff` mặc định 10 m; `goto` mặc định giữ độ cao) |
| `lat`, `lon` | với `goto` | độ |

## Kết quả (JSON trên `drone/001/cmd/ack`)

```json
{ "cmd_id": 1794700000123, "from": "op1", "action": "goto",
  "result": "accepted", "reason": "", "owner": "op1",
  "sent_ms": 1794700000123, "gw_recv_ms": 1794700000125, "ack_ms": 1794700000126 }
```

`result` là `accepted` hoặc `rejected`. Khi bị từ chối, `reason` là một trong:

| reason | Nghĩa |
|---|---|
| `retained` | lệnh là tin retained (kiểm tra 1) |
| `expired` | lệnh tới Gateway sau hạn `ttl_ms` (kiểm tra 2) |
| `duplicate` | đã nhận lệnh có cùng `from` + `cmd_id` (kiểm tra 3) |
| `not_owner` | người khác đang giữ quyền điều khiển (kiểm tra 4) |
| `invalid` | JSON sai hoặc thiếu trường |
| `invalid_state` | lệnh không hợp lúc này (cất cánh khi đang bay, hạ cánh khi đã ở đất...) |
| `out_of_range` | độ cao ngoài 1–120 m hoặc điểm `goto` cách nhà quá 1 km |
| `px4_...` | PX4 từ chối (chỉ khi chạy với PX4 thật) |

## Bốn kiểm tra an toàn của Gateway

| # | Nguy cơ do tính năng MQTT | Cách chặn |
|---|---|---|
| 1 | **Retained**: Broker gửi lại tin retained mỗi khi Gateway subscribe, tức là mỗi lần kết nối lại → lệnh cũ chạy lại | Từ chối mọi lệnh có cờ retained |
| 2 | **Broker giữ tin QoS 1** khi Gateway vắng mặt → lệnh gửi lúc mất kết nối tới muộn hàng chục giây | Lệnh có hạn dùng; `gw_recv_ms − sent_ms > ttl_ms` thì từ chối |
| 3 | **QoS 1 "ít nhất một lần"**: sau khi kết nối lại, tin chưa được PUBACK bị gửi lại (cờ DUP) → lệnh chạy hai lần | Nhớ `(from, cmd_id)` đã xử lý, bỏ lệnh trùng |
| 4 | **Ai cũng publish được** → hai người cùng điều khiển một drone | Người gửi lệnh hợp lệ đầu tiên giữ quyền; người giữ im lặng quá 5 s thì quyền tự nhả (giống "proof of life" của PX4 Offboard) |

Kiểm tra 2 cần đồng hồ máy người gửi và máy Gateway khớp nhau (NTP). Khi đo, `commander` chạy cùng máy
với Gateway. App trên điện thoại cần đồng hồ tự động đồng bộ mạng.

Gateway ghi mọi lệnh vào `cmd_log.csv`:
`gw_recv_ms, from, cmd_id, action, sent_ms, age_ms, retained, dup_flag, result, reason`.

## Cách thử

```bash
./build/gateway/gateway --source fake --out /tmp/cmd          # drone giả lập, đang hover 25,5 m
./build/commander/commander --action land                      # hạ cánh
./build/commander/commander --action takeoff --alt 15          # cất cánh lên 15 m
./build/commander/commander --action goto --lat 47.3980 --lon 8.5460
./build/commander/commander --action rtl                       # quay về nhà và hạ cánh
```

Thử từng kiểm tra:

```bash
./build/commander/commander --action hold --age-ms 10000       # 2: expired
./build/commander/commander --action hold --repeat-id          # 3: lần hai bị duplicate
./build/commander/commander --action hold --from op2           # 4: not_owner (trong 5 s sau op1)
./build/commander/commander --action hold --retain --ttl-ms 600000
#   rồi tắt và bật lại Gateway                                   1: lệnh retained bị từ chối
```

Gateway chạy `--cmd-safety off` để tắt cả 4 kiểm tra, dùng làm đối chứng trong thí nghiệm H.

## Thí nghiệm H (đề xuất)

- **H1, khứ hồi lệnh:** `commander --action hold --count 300 --interval-ms 200 --qos 0|1 --out data/H/...`
  dưới 4 điều kiện mạng N0–N3; đo `rtt_ms` (gửi → nhận ack), tách `down_ms` và `up_ms`.
- **H2, lệnh trễ:** tắt Gateway, gửi lệnh `goto`, 15 s sau bật lại Gateway. Có kiểm tra:
  `expired`, drone không bay. `--cmd-safety off`: drone bay theo lệnh cũ 15 s.
- **H3, lệnh trùng:** Broker dừng trong lúc gửi chuỗi lệnh QoS 1; đếm `dup_flag = 1` và số lệnh bị
  `duplicate` trong `cmd_log.csv`.
