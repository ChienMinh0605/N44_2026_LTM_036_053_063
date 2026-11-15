## C – Broker tắt 15 s giữa lượt (QoS 1): bộ đệm Gateway tắt và bật

| Thư mục | Gửi | sent / buffered / dropped | Nhận (duy nhất) | Mất | Trùng | Gateway kết nối lại sau (s) | Tin xả bù: trễ trung vị / lớn nhất (ms) |
|---|---|---|---|---|---|---|---|
| buffer-off_run1 | 3588 | 3188 / 0 / 400 | 3188 | 400 | 0 | 19.9 | - / - |
| buffer-off_run2 | 3608 | 3208 / 0 / 400 | 3208 | 400 | 0 | 20.0 | - / - |
| buffer-off_run3 | 3612 | 3212 / 0 / 400 | 3212 | 400 | 1 | 20.1 | - / - |
| buffer-on_run1 | 3592 | 3196 / 396 / 0 | 3592 | 0 | 1 | 19.9 | 9901 / 19687 |
| buffer-on_run2 | 3596 | 3196 / 400 / 0 | 3596 | 0 | 2 | 20.0 | 10053 / 19824 |
| buffer-on_run3 | 3608 | 3260 / 348 / 0 | 3608 | 0 | 1 | 17.5 | 10418 / 19001 |

## F – Logger dừng 15 s rồi chạy lại cùng client_id: QoS 0 và QoS 1

| Thư mục | Gửi | Nhận (duy nhất) | Mất | Trùng | Gửi lúc Logger tắt | Nhận bù | Trễ tin nhận bù: trung vị / lớn nhất (ms) |
|---|---|---|---|---|---|---|---|
| qos0_run1 | 3604 | 3304 | 300 | 0 | 300 | 0 | - / - |
| qos0_run2 | 3604 | 3300 | 304 | 0 | 304 | 0 | - / - |
| qos0_run3 | 3604 | 3304 | 300 | 0 | 300 | 0 | - / - |
| qos1_run1 | 3584 | 3584 | 0 | 0 | 300 | 300 | 7603 / 14993 |
| qos1_run2 | 3604 | 3604 | 0 | 0 | 304 | 304 | 7641 / 15016 |
| qos1_run3 | 3584 | 3584 | 0 | 0 | 304 | 304 | 7638 / 15017 |
