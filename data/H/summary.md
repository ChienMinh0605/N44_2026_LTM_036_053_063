## H1 – Thời gian khứ hồi lệnh (gộp 3 lần chạy)

| Điều kiện | QoS | Số lệnh | accepted | timeout | Khứ hồi trung vị (ms) | p95 (ms) | Lớn nhất (ms) | Đi trung vị (ms) | Về trung vị (ms) | Lệnh nhận >1 ack |
|---|---|---|---|---|---|---|---|---|---|---|
| N0 | 0 | 900 | 900 | 0 | 15 | 48 | 64 | 1 | 0 | 0 |
| N0 | 1 | 900 | 900 | 0 | 42 | 68 | 324 | 1 | 29 | 0 |
| N1 | 0 | 900 | 900 | 0 | 378 | 489 | 531 | 202 | 182 | 0 |
| N1 | 1 | 900 | 900 | 0 | 457 | 496 | 642 | 196 | 260 | 0 |
| N2 | 0 | 900 | 900 | 0 | 16 | 242 | 617 | 1 | 0 | 0 |
| N2 | 1 | 900 | 900 | 0 | 43 | 383 | 1214 | 1 | 30 | 0 |
| N3 | 0 | 900 | 900 | 0 | 426 | 889 | 1671 | 237 | 189 | 0 |
| N3 | 1 | 900 | 900 | 0 | 484 | 959 | 1785 | 191 | 251 | 0 |

Từng lần chạy:

| Thư mục | Số lệnh | timeout | Trung vị (ms) | p95 (ms) |
|---|---|---|---|---|
| N0_qos0_run1 | 300 | 0 | 47 | 53 |
| N0_qos0_run2 | 300 | 0 | 15 | 16 |
| N0_qos0_run3 | 300 | 0 | 10 | 15 |
| N0_qos1_run1 | 300 | 0 | 42 | 42 |
| N0_qos1_run2 | 300 | 0 | 42 | 46 |
| N0_qos1_run3 | 300 | 0 | 67 | 68 |
| N1_qos0_run1 | 300 | 0 | 380 | 484 |
| N1_qos0_run2 | 300 | 0 | 374 | 489 |
| N1_qos0_run3 | 300 | 0 | 385 | 477 |
| N1_qos1_run1 | 300 | 0 | 459 | 496 |
| N1_qos1_run2 | 300 | 0 | 462 | 498 |
| N1_qos1_run3 | 300 | 0 | 435 | 477 |
| N2_qos0_run1 | 300 | 0 | 16 | 290 |
| N2_qos0_run2 | 300 | 0 | 16 | 232 |
| N2_qos0_run3 | 300 | 0 | 21 | 242 |
| N2_qos1_run1 | 300 | 0 | 42 | 305 |
| N2_qos1_run2 | 300 | 0 | 44 | 399 |
| N2_qos1_run3 | 300 | 0 | 43 | 419 |
| N3_qos0_run1 | 300 | 0 | 450 | 933 |
| N3_qos0_run2 | 300 | 0 | 425 | 864 |
| N3_qos0_run3 | 300 | 0 | 410 | 868 |
| N3_qos1_run1 | 300 | 0 | 493 | 956 |
| N3_qos1_run2 | 300 | 0 | 448 | 971 |
| N3_qos1_run3 | 300 | 0 | 489 | 914 |

## H2 – Lệnh goto tới Gateway muộn

| Thư mục | Kiểm tra | Lệnh bị giữ (ms) | Kết quả | Lý do | Tốc độ lớn nhất sau khi bật lại (m/s) | Drone bay? |
|---|---|---|---|---|---|---|
| h2_off_run1 | off | 11934 | accepted | - | 5.2 | có |
| h2_off_run2 | off | 11940 | accepted | - | 5.2 | có |
| h2_off_run3 | off | 11511 | accepted | - | 5.2 | có |
| h2_on_run1 | on | 11954 | rejected | expired | 0.0 | không |
| h2_on_run2 | on | 11666 | rejected | expired | 0.0 | không |
| h2_on_run3 | on | 11733 | rejected | expired | 0.0 | không |

## H3 – Broker khởi động lại giữa chuỗi 100 lệnh QoS 1

| Thư mục | Lệnh tới Gateway | dup_flag = 1 | duplicate | expired | accepted | Kết quả phía commander |
|---|---|---|---|---|---|---|
| h3_run1 | 101 | 1 | 0 | 36 | 65 | Tong: gui 100 lenh / accepted=65 rejected:expired=35 / khu hoi trung vi 357 ms, p95 9227 ms |
| h3_run2 | 100 | 0 | 0 | 26 | 74 | Tong: gui 100 lenh / accepted=74 rejected:expired=26 / khu hoi trung vi 43 ms, p95 7295 ms |
| h3_run3 | 101 | 1 | 0 | 35 | 66 | Tong: gui 100 lenh / accepted=66 rejected:expired=34 / khu hoi trung vi 182 ms, p95 9040 ms |
