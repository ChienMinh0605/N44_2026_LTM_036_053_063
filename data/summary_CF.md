## C – Broker tắt 15 s giữa lượt (QoS 1): bộ đệm Gateway tắt và bật

| Thư mục | Gửi | sent / buffered / dropped | Nhận (duy nhất) | Mất | Trùng | Gateway kết nối lại sau (s) | Tin xả bù: trễ trung vị / lớn nhất (ms) |
|---|---|---|---|---|---|---|---|
| buffer-off_run1 | 3584 | 3044 / 0 / 540 | 3044 | 540 | 0 | 26.9 | - / - |
| buffer-off_run2 | 3572 | 3212 / 0 / 360 | 3212 | 360 | 0 | 18.0 | - / - |
| buffer-off_run3 | 3588 | 3228 / 0 / 360 | 3228 | 360 | 1 | 18.1 | - / - |
| buffer-on_run1 | 3592 | 3224 / 368 / 0 | 3592 | 0 | 0 | 18.3 | 10119 / 19027 |
| buffer-on_run2 | 3580 | 3208 / 372 / 0 | 3580 | 0 | 0 | 18.6 | 10098 / 19095 |
| buffer-on_run3 | 3564 | 3020 / 544 / 0 | 3564 | 0 | 0 | 27.1 | 14566 / 27950 |

## F – Logger dừng 15 s rồi chạy lại cùng client_id: QoS 0 và QoS 1

| Thư mục | Gửi | Nhận (duy nhất) | Mất | Trùng | Gửi lúc Logger tắt | Nhận bù | Trễ tin nhận bù: trung vị / lớn nhất (ms) |
|---|---|---|---|---|---|---|---|
| qos0_run1 | 3596 | 3292 | 304 | 0 | 304 | 0 | - / - |
| qos0_run2 | 3576 | 3272 | 304 | 0 | 304 | 0 | - / - |
| qos0_run3 | 3556 | 3252 | 304 | 0 | 304 | 0 | - / - |
| qos1_run1 | 3596 | 3596 | 0 | 0 | 300 | 300 | 7582 / 14971 |
| qos1_run2 | 3596 | 3596 | 0 | 0 | 304 | 304 | 7674 / 15066 |
| qos1_run3 | 3596 | 3596 | 0 | 0 | 304 | 304 | 7683 / 15075 |
