# Thí nghiệm C và F

Chạy trên một máy Linux: Broker Mosquitto (Docker, 172.28.0.10), Gateway (4 topic × 5 tin/s
= 20 tin/s), Logger C++. Mỗi lượt Gateway phát 180 s, sự cố ở giây thứ 60, kéo dài 15 s; Logger chạy trước
3 s, nhận thêm 15 s sau khi Gateway dừng, và được xoá phiên cũ trước mỗi lượt. Mỗi trường hợp 3 lần, thứ tự
hai mức đảo giữa các lần.

Chạy lại: `./scripts/run_CF.sh all` (khoảng 40 phút), tổng hợp: `./scripts/summarize_CF.sh`.
Số liệu gốc: `data/C/`, `data/F/`, bảng đầy đủ: [data/summary_CF.md](../data/summary_CF.md).

Một tin được định danh bằng `(boot_id, topic, seq)`; "Mất" = gửi mà Logger không nhận được lần nào.

## C – Tắt Broker 15 s: bộ đệm Gateway tắt và bật (QoS 1)

| Bộ đệm | Gửi (TB) | Bị bỏ ngay ở Gateway | Mất | Tin trùng | Gateway kết nối lại sau | Tin xả bù: trễ trung vị / lớn nhất |
|---|---|---|---|---|---|---|
| Tắt | 3603 | 400 (cả 3 lần) | 400 (11,1 %) | 0–1 | 19,9–20,1 s | – |
| Bật | 3599 | 0 | **0** | 1–2 | 17,5–20,0 s | ≈ 10 s / ≈ 19,5 s |

**Nhận xét:**
- **Bộ đệm tắt:** suốt lúc mất kết nối, `publish()` bị Paho từ chối, Gateway ghi `dropped` rồi chạy tiếp
  (không dừng). 400 tin = 20 tin/s × 20 s mất kết nối. Không tin nào trong số đó tới được Logger.
- **Bộ đệm bật:** tin được giữ trong RAM của Gateway (tối đa 1000 tin), kết nối lại thì xả hết.
  Không mất tin nào. Đổi lại, tin xả bù tới trễ: tin vào bộ đệm đầu tiên phải chờ gần 20 s.
- **Kết nối lại chậm hơn Broker khoảng 2–5 s:** Broker sẵn sàng sau ≈ 15,4 s, nhưng Gateway tự kết nối lại theo
  nhịp chờ 1, 2, 4, 8, 8… s (`kReconnectMinSec`/`kReconnectMaxSec` trong `config.hpp`), nên phải đợi tới lần thử
  kế tiếp. Muốn nhanh hơn thì giảm thời gian chờ tối đa, đổi lại client thử kết nối dày hơn khi Broker hỏng lâu.
- **Tin trùng 0–2:** là tin QoS 1 đã gửi đi nhưng chưa nhận PUBACK lúc Broker tắt. Sau khi kết nối lại, Paho gửi lại
  tin đó (cờ DUP) vì `clean_session = false`. Đây đúng là "ít nhất một lần" của QoS 1.

## F – Dừng Logger 15 s rồi chạy lại cùng client_id (Broker vẫn chạy)

| QoS | Gửi (TB) | Gửi lúc Logger tắt | Nhận bù | Mất | Tin trùng | Trễ tin nhận bù: trung vị / lớn nhất |
|---|---|---|---|---|---|---|
| 0 | 3604 | 300–304 | **0** | 300–304 (8,3–8,4 %) | 0 | – |
| 1 | 3591 | 300–304 | **300–304 (100 %)** | 0 | 0 | ≈ 7,6 s / ≈ 15 s |

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
