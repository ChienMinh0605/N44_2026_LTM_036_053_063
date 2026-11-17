# Thí nghiệm E – QoS 0 và QoS 1 khi mạng xấu

Chạy trên một máy Linux: PX4 SITL v1.17.0 + Gazebo (drone x500 **hover ở 10 m** trong mọi lượt, kiểm tra bằng
`scripts/hover.sh` trước mỗi lượt, kết quả trong `hover.txt`), Broker Mosquitto (Docker, 172.28.0.10), Gateway đọc PX4
qua MAVSDK (4 topic × 5 tin/s = 20 tin/s), Logger C++ cùng máy với Gateway (chung đồng hồ, nên độ trễ đo được là thật).
2 QoS × 4 điều kiện mạng × 3 lần = **24 lượt**, mỗi lượt Gateway phát 180 s, Logger nhận thêm 15 s.
QoS chạy xen kẽ: lần 1 và 3 chạy QoS 0 trước, lần 2 chạy QoS 1 trước.

| Điều kiện | netem trên eth0 của Broker | ping kiểm tra (50 gói) |
|---|---|---|
| N0 | không | RTT 0,08 ms, mất 0 % |
| N1 | trễ 100 ms | RTT 100,1 ms, mất 0 % |
| N2 | mất 5 % | RTT 0,06 ms, mất 6,3 % |
| N3 | trễ 100 ms + mất 5 % | RTT 100,1 ms, mất 2,7 % |

netem chỉ làm xấu **chiều Broker → client** (tin tới Logger, PUBACK về Gateway); chiều Gateway → Broker không
bị ảnh hưởng. "Trễ 100 ms" là thêm 100 ms một chiều.

Chạy lại: PX4 SITL đang chạy, rồi `./scripts/run_all_E.sh` (khoảng 85 phút, cần sudo cho netem). Số liệu gốc: `data/E/`.
Bảng và biểu đồ do `b_analyze` sinh ra: `data/plots/` (`per_run.csv`, `summary.csv`, `cdf.svg`, `receive_rate.svg`).
Quy tắc tính: bỏ 10 s đầu theo giờ publish; một tin được định danh bằng `(boot_id, topic, seq)`; trung vị và
p95 lấy phần tử thứ n·50/100 và n·95/100 (giống thí nghiệm H).

## Kết quả (trung bình 3 lần ± độ lệch chuẩn)

| Điều kiện | QoS | Tỷ lệ nhận | Tin trùng | Trễ trung vị (ms) | Trễ p95 (ms) |
|---|---|---|---|---|---|
| N0 | 0 | 100 % | 0 | 0 ± 0 | 2,3 ± 1,2 |
| N0 | 1 | 100 % | 0 | 0,3 ± 0,6 | 1,3 ± 0,6 |
| N1 | 0 | 100 % | 0 | 200 ± 0 | 300 ± 1 |
| N1 | 1 | 100 % | 0 | 145 ± 4 | 340 ± 1 |
| N2 | 0 | 100 % | 0 | 0,7 ± 0,6 | 231 ± 9 |
| N2 | 1 | 100 % | 0 | 1 ± 1 | 212 ± 4 |
| N3 | 0 | 100 % | 0 | 204 ± 2 | 536 ± 7 |
| N3 | 1 | 100 % | 0 | 240 ± 1 | 557 ± 12 |

Số liệu đo lại bằng PX4 SITL ngày 17/11/2026. Bộ số cũ đo bằng nguồn dữ liệu giả (đã bỏ khỏi repo) còn trong
lịch sử git ở commit `5dd7169`. Tỷ lệ nhận, tin trùng, N0 và N2 gần như không đổi. Thay đổi đáng kể: N1 QoS 0
trung vị 147 → 200 ms, N1 QoS 1 175 → 145 ms (chênh lệch đổi chiều, xem ý 4), N3 p95 573/589 → 536/557 ms.

![Tỷ lệ nhận](../data/plots/receive_rate.svg)
![CDF độ trễ](../data/plots/cdf.svg)

## Nhận xét

1. **Không mất tin nào, kể cả QoS 0 ở điều kiện mất 5 %.** Mất gói ở đây là mất **gói TCP**, không phải mất bản tin
   MQTT. Kết nối TCP vẫn sống nên TCP tự truyền lại gói bị mất; bản tin chỉ tới muộn hơn. Không lượt nào bị mất
   kết nối (`events.csv` chỉ có một lần `connected` mỗi lượt), nên QoS 0 cũng nhận đủ 100 %.

2. **Không có tin trùng ở cả QoS 1.** Đúng như kế hoạch dự đoán: trên một kết nối đang sống, MQTT 3.1.1 không gửi
   lại PUBLISH; client chỉ gửi lại tin QoS 1 (cờ DUP) sau khi kết nối lại. Tin trùng chỉ xuất hiện khi có mất kết
   nối, như ở thí nghiệm C (0–1 tin mỗi lượt).

3. **Mạng xấu làm tăng độ trễ, không làm mất tin.**
   - N2 (mất 5 %): trung vị vẫn khoảng 1 ms, nhưng p95 lên khoảng 210–230 ms. Phần lớn tin tới ngay, khoảng 5 %
     tin dính gói bị mất phải chờ TCP truyền lại (thời gian chờ tối thiểu của Linux khoảng 200 ms).
   - N1 (trễ 100 ms): mọi tin chậm ít nhất 100 ms, trung vị 145–200 ms, p95 300–340 ms.
   - N3 cộng dồn cả hai: p95 khoảng 540–560 ms.

4. **QoS 1 không giúp gì thêm khi kết nối còn sống.** Trung vị QoS 0 và QoS 1 chênh nhau, nhưng chiều chênh lệch
   **không cố định**: ở N1 QoS 0 chậm hơn (200 so với 145 ms), ở N3 QoS 1 chậm hơn (240 so với 204 ms); bộ số cũ
   thì ở N1 QoS 1 chậm hơn. Chênh lệch này đến từ thuật toán Nagle (ý 5), không phải từ QoS: khi tắt Nagle, QoS 0 và QoS 1 trễ như nhau
   (102 và 103 ms). Lợi ích của QoS 1 chỉ thấy rõ khi **mất hẳn kết nối** (thí nghiệm C và F), nơi QoS 1 + phiên
   bền giúp nhận bù 100 % thay vì mất hết.

5. **Trễ trung vị ở N1 (145–200 ms) lớn hơn 100 ms netem thêm vào là do thuật toán Nagle của TCP trên Broker
   (đã kiểm chứng).** Nagle gom các gói nhỏ lại: khi gói trước chưa được ACK, gói tiếp theo phải chờ. Đường bị trễ
   100 ms làm ACK về chậm, nên nhiều tin phải chờ thêm. Mosquitto mặc định không tắt Nagle (`set_tcp_nodelay false`).
   Kiểm chứng bằng cách chạy lại N1, mỗi lần 70 s (bỏ 10 s đầu, khoảng 1200 tin):

   | `set_tcp_nodelay` | QoS | Trễ trung vị | Trễ p95 |
   |---|---|---|---|
   | false (mặc định, Nagle bật) | 0 | 202 ms | 301 ms |
   | false (mặc định, Nagle bật) | 1 | 145 ms | 343 ms |
   | true (Nagle tắt) | 0 | **102 ms** | 202 ms |
   | true (Nagle tắt) | 1 | **103 ms** | 202 ms |

   Tắt Nagle thì trung vị về đúng khoảng 100 ms netem thêm vào, và hết chênh lệch giữa QoS 0 và QoS 1. Khi Nagle
   bật, trung vị dao động mạnh (145–202 ms) tuỳ thời điểm tin rơi vào lúc chờ ACK.

   **p95 vẫn khoảng 200 ms dù tắt Nagle trên Broker, vì còn Nagle ở socket của Gateway** (đã xác nhận bằng
   Wireshark, `data/pcap/6_N1_nodelay.pcap`: N1, QoS 1, Broker `set_tcp_nodelay true`, 392 tin sau 10 s đầu).
   Mỗi 200 ms Gateway publish liền 4 tin (position, altitude, velocity, battery). Tin đầu lên dây ngay; ba tin
   sau là gói nhỏ, phải chờ Broker ACK tin đầu, mà ACK (đi kèm PUBACK) bị netem làm trễ 100 ms. Khi ACK tới,
   ba tin được gộp vào **một** gói TCP (ví dụ gói 793). Tách độ trễ từng tin theo thời điểm trên dây:

   | Đoạn | Trung vị | p95 |
   |---|---|---|
   | Trong Gateway (từ `timestamp_ms` tới lúc lên dây) | 1 ms | 101 ms |
   | Broker + netem (Gateway → Broker tới Broker → Logger) | 100 ms | 100 ms |
   | Tổng | 101 ms | 201 ms |

   Khoảng một nửa số chu kỳ, ACK của chu kỳ trước về đúng lúc nên cả 4 tin đi ngay. Vì vậy khoảng 37 % số tin
   chờ thêm 100 ms: không đủ để kéo trung vị, nhưng đủ làm p95 thành 200 ms. Thư viện Paho mà Gateway dùng không
   đặt `TCP_NODELAY`, nên muốn hết phần này phải tắt Nagle ngay trên socket của Gateway.
   Nhóm giữ cấu hình mặc định cho toàn bộ thí nghiệm. Với telemetry cần trễ thấp thì nên bật
   `set_tcp_nodelay true` trên Broker.

## Kết luận

Với telemetry gửi đều 20 tin/s qua một kết nối TCP còn sống, **TCP đã đảm bảo không mất tin**; mạng xấu chỉ làm
tin tới muộn hơn (p95 tới khoảng 0,6 s ở N3), và thuật toán Nagle (trên Broker và trên socket của Gateway) còn cộng thêm khoảng
50–100 ms khi đường truyền bị trễ. QoS 1 không cải thiện tỷ lệ nhận trong trường hợp này. QoS 1 chỉ đáng dùng khi có nguy cơ **mất kết nối**, và phải kết hợp với phiên bền (`clean_session = false`)
hoặc bộ đệm phía client mới có tác dụng (xem thí nghiệm C, F).
