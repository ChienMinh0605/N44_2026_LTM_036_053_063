# Thí nghiệm C và F

Chạy trên một máy Linux: PX4 SITL v1.17.0 + Gazebo (drone x500 hover ở 10 m, kiểm tra bằng `scripts/hover.sh`
trước mỗi lượt), Broker Mosquitto (Docker, 172.28.0.10), Gateway đọc PX4 qua MAVSDK (4 topic × 5 tin/s = 20 tin/s),
Logger C++. Đo lại ngày 17/11/2026; bộ số cũ đo bằng nguồn dữ liệu giả còn trong git ở commit `5dd7169`. Mỗi lượt Gateway phát 180 s, sự cố ở giây thứ 60, kéo dài 15 s; Logger chạy trước
3 s, nhận thêm 15 s sau khi Gateway dừng, và được xoá phiên cũ trước mỗi lượt. Mỗi trường hợp 3 lần, thứ tự
hai mức đảo giữa các lần.

Chạy lại: PX4 SITL đang chạy, rồi `./scripts/run_CF.sh all` (khoảng 42 phút), tổng hợp: `./scripts/summarize_CF.sh`.
Số liệu gốc: `data/C/`, `data/F/`, bảng đầy đủ: [data/summary_CF.md](../data/summary_CF.md).

Một tin được định danh bằng `(boot_id, topic, seq)`; "Mất" = gửi mà Logger không nhận được lần nào.

## C – Tắt Broker 15 s: bộ đệm Gateway tắt và bật (QoS 1)

| Bộ đệm | Gửi (TB) | Bị bỏ ngay ở Gateway | Mất | Tin trùng | Gateway kết nối lại sau | Tin xả bù: trễ trung vị / lớn nhất |
|---|---|---|---|---|---|---|
| Tắt | 3581 | 540 / 360 / 360 | 360–540 (10,0–15,1 %) | 0–1 | 18,0 / 18,1 / 26,9 s | – |
| Bật | 3579 | 0 | **0** | 0 | 18,3 / 18,6 / 27,1 s | ≈ 10 s / ≈ 19 s (lượt 3: 14,6 s / 28,0 s) |

So với bộ số cũ (mất 400 tin cả 3 lần, kết nối lại sau 19,9–20,1 s): số tin mất giờ dao động 360–540 vì thời gian
kết nối lại dao động (18 s hoặc 27 s, xem nhận xét). Kết luận không đổi: bộ đệm tắt thì mất, bộ đệm bật thì không.

**Nhận xét:**
- **Bộ đệm tắt:** suốt lúc mất kết nối, `publish()` bị Paho từ chối, Gateway ghi `dropped` rồi chạy tiếp
  (không dừng). Số tin mất = 20 tin/s × thời gian mất kết nối: 360 tin ứng với 18 s, 540 tin ứng với 27 s. Không tin nào trong số đó tới được Logger.
- **Bộ đệm bật:** tin được giữ trong RAM của Gateway (tối đa 1000 tin), kết nối lại thì xả hết.
  Không mất tin nào. Đổi lại, tin xả bù tới trễ: tin vào bộ đệm đầu tiên phải chờ bằng cả thời gian mất kết nối
  (19 s, lượt 3 là 28 s).
- **Kết nối lại chậm hơn Broker 2,5–11,5 s.** Broker sẵn sàng sau ≈ 15,5 s, nhưng Gateway chỉ kết nối lại ở lần tự
  thử đầu tiên **bắt đầu sau** lúc đó. Để thấy lịch thử, nhóm chạy riêng 4 lượt Gateway dưới `strace` (Broker tắt
  15 s, log trong `data/C/reconnect_strace/`):
  - Trong lúc Broker tắt, mỗi lần thử bị treo khoảng **3,1 s** rồi nhận lỗi `EHOSTUNREACH`. Container đã dừng nên
    không ai trả lời ARP cho 172.28.0.10; Linux bỏ cuộc sau 3 lần hỏi ARP, sớm hơn connect timeout 5 s của Paho
    (`mqtt_link.cpp`). Nếu Broker chỉ tắt tiến trình mà máy vẫn còn, lần thử sẽ bị từ chối ngay.
  - Các lần thử bắt đầu ở +3,9 s, +9,9–10,1 s và +19,1–19,4 s sau khi mất kết nối. Khoảng cách giữa hai lần thử
    tăng dần (6 s rồi 9,2 s): mỗi lần thử treo 3 s, rồi Paho chờ lâu dần trước lần sau (khoảng 3 s rồi 6 s; cấu
    hình chờ lùi dần 1 s → tối đa 8 s là `kReconnectMinSec` / `kReconnectMaxSec` trong `config.hpp`).
  - Cả 4 lượt đều kết nối lại sau 20,0–20,2 s, trùng với bộ số cũ.
  - Trong 6 lượt C, thời gian kết nối lại lại là 18 s hoặc 27 s. 27 s hợp với trường hợp một lần thử bắt đầu ngay
    trước khi Broker sẵn sàng, thất bại, rồi phải chờ lần kế tiếp khoảng 9 s sau. **Chưa giải thích được** vì sao
    lịch thử trong các lượt C khác với lượt `strace` (thời điểm lần thử đầu không giống nhau); cần bắt gói hoặc
    `strace` ngay trong lượt C để biết.
  Muốn kết nối lại nhanh hơn thì giảm thời gian chờ tối đa, đổi lại client thử kết nối dày hơn khi Broker hỏng lâu.
- **Tin trùng 0–1:** là tin QoS 1 đã gửi đi nhưng chưa nhận PUBACK lúc Broker tắt. Sau khi kết nối lại, Paho gửi lại
  tin đó (cờ DUP) vì `clean_session = false`. Đây đúng là "ít nhất một lần" của QoS 1.

## F – Dừng Logger 15 s rồi chạy lại cùng client_id (Broker vẫn chạy)

| QoS | Gửi (TB) | Gửi lúc Logger tắt | Nhận bù | Mất | Tin trùng | Trễ tin nhận bù: trung vị / lớn nhất |
|---|---|---|---|---|---|---|
| 0 | 3576 | 304 | **0** | 304 (8,5 %) | 0 | – |
| 1 | 3596 | 300–304 | **300–304 (100 %)** | 0 | 0 | ≈ 7,6 s / ≈ 15 s |

So với bộ số cũ (QoS 0 mất 300–304 tin, 8,3–8,4 %): gần như không đổi.

**Nhận xét:**
- Logger dùng `clean_session = false` nên Broker **giữ phiên** khi Logger vắng mặt: nhớ subscription và xếp hàng
  tin cho Logger. Logger chạy lại với cùng `client_id` thì nhận lại phiên đó.
- **QoS 1:** Broker xếp hàng mọi tin trong 15 s Logger vắng, chạy lại là nhận bù đủ 100 %. Trễ lớn nhất ≈ 15 s
  đúng bằng thời gian Logger tắt (tin đầu tiên phải chờ lâu nhất).
- **QoS 0:** Broker không xếp hàng tin QoS 0 cho client đang vắng (`queue_qos0_messages false` trong
  `mosquitto.conf`, cũng là mặc định của Mosquitto), nên mất hết 15 s × 20 tin/s = 300 tin.
- Broker giữ tối đa 5000 tin cho mỗi client (`max_queued_messages 5000`); 15 s chỉ có 300 tin nên chưa chạm giới
  hạn. Logger tắt quá 250 s (5000 / 20) thì phần cũ hơn sẽ bị bỏ dù dùng QoS 1.

## So sánh C và F

Cả hai đều chống mất tin, nhưng ở hai chỗ khác nhau:

| | C – bộ đệm Gateway | F – phiên bền trên Broker |
|---|---|---|
| Sự cố | Broker (hoặc đường tới Broker) hỏng | Một client nhận tạm vắng |
| Ai giữ tin | Gateway (RAM, tối đa 1000 tin) | Broker (tối đa 5000 tin mỗi client) |
| Điều kiện | `--buffer on` | QoS 1 + `clean_session = false` + cùng `client_id` |
| Cái giá | Tin xả bù tới trễ, mất hết nếu Gateway tắt khi còn tin trong bộ đệm | Tin nhận bù tới trễ, Broker tốn bộ nhớ cho client vắng |
