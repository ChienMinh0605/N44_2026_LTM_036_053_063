# App Flutter (Tiên)

Tạo project ngay trong thư mục này:

```bash
cd app
flutter create --project-name drone_monitor .
flutter pub add mqtt_client
```

Quy ước kết nối:

- Broker: IP của laptop, cổng 1883, MQTT 3.1.1, tài khoản `drone` / `drone123`
- Subscribe **từng topic riêng**, không dùng wildcard `drone/001/#` (QoS 1 cho tất cả):
  - luôn nhận `drone/001/status` và `drone/001/cmd/ack` (kết quả lệnh điều khiển);
  - 4 topic số liệu `drone/001/position`, `altitude`, `velocity`, `battery` chỉ subscribe khi ô tương ứng
    được tích; bỏ tích thì unsubscribe. Thí nghiệm B chỉ tích `battery`.
  - App không subscribe `drone/001/cmd` nên không nhận lại lệnh của chính mình hay của người khác.
- Topic `drone/001/status` là retained: vừa kết nối là biết Gateway đang online hay offline
- Định dạng JSON: xem mục 2 của kế hoạch trong `docs/`

Nếu Docker chạy trong WSL2 mà điện thoại không kết nối được: bật `networkingMode=mirrored`
trong `.wslconfig` (hoặc dùng `netsh interface portproxy`) và mở cổng 1883 trên Windows Firewall.
Không được thì chạy app trên desktop hoặc emulator ngay trên laptop.

## Điều khiển drone (kênh lệnh)

Giao thức: `docs/command_channel.md`. App gửi lệnh lên `drone/001/cmd` (QoS 1, **không retained**,
`from = app-tien`, `ttl_ms = 3000`) và nhận kết quả trên `drone/001/cmd/ack`.

- Nút: **Cất cánh** (ô độ cao, mặc định 10 m), **Hạ cánh**, **Quay về**, **Đứng yên**, **Bay tới**
  (ô lat/lon, nút đặt sẵn điểm demo `47.3980, 8.5460`), **Nhả quyền**.
- Ô kết quả: `accepted` (xanh) hoặc `rejected: <reason>` (đỏ), kèm khứ hồi (đi Gateway / về App).
  Quá 3 giây không có ack thì hiện "Không có phản hồi". App không tự gửi lại; bấm lại là lệnh mới.
- Dòng "Đang giữ quyền" lấy từ trường `owner` của mọi ack, kể cả ack của người điều khiển khác.
- Mất kết nối Broker thì các nút bị khoá.
- Điện thoại thật phải để đồng hồ tự động đồng bộ, nếu lệch quá 3 s lệnh sẽ bị `expired`.

Test với Broker và Gateway thật (lái App qua các tình huống trong `docs/tasks/tien_kenh_lenh.md`):

```bash
./build/gateway/gateway --out /tmp/cmd &      # ở thư mục gốc repo, PX4 SITL đang chạy
cd app && flutter test test/cmd_channel_live_test.dart --dart-define=LIVE=true
```
