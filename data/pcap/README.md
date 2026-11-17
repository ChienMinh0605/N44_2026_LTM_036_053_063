# Bắt gói MQTT (Wireshark)

Bắt bằng `scripts/capture_pcap.sh` trên bridge Docker của mạng `mqttnet`, lọc `tcp port 1883`. Hai client là
Gateway (`gateway-01`) và Logger (`logger-01`), Broker ở 172.28.0.10. Chạy lại script phải dùng **terminal thường của
máy**, không dùng tab Terminal của app Claude: AppArmor chặn mọi tín hiệu từ đó tới tcpdump, nên tcpdump không dừng được.

Gói CONNECT chứa tài khoản MQTT dạng chữ thường (`drone` / `drone123`, đã có trong repo). Hệ thống thật cần TLS.

| File | Kịch bản | Thấy được gì |
|---|---|---|
| `1_qos1.pcap` | Logger + Gateway QoS 1, 3 s, tắt bằng Ctrl+C | CONNECT/CONNACK, SUBSCRIBE/SUBACK, PUBLISH QoS 1 + PUBACK, Gateway tự báo `offline` rồi DISCONNECT |
| `2_qos0.pcap` | Như trên nhưng QoS 0 | PUBLISH QoS 0 **không có PUBACK** (PUBACK duy nhất là của tin `status` QoS 1) |
| `3_mat_goi_N2.pcap` | netem N2 (mất 5 %), QoS 1, 30 s | 31 lỗ hổng sequence (gói netem bỏ), 33 gói TCP truyền lại, 15 Dup ACK; MQTT vẫn đủ 1201 PUBLISH |
| `4_last_will.pcap` | Gateway bị `kill -9` | Không có DISCONNECT của Gateway; Broker tự phát Last Will `offline` tới Logger (gói 255) |
| `5_lenh_qos1_N0.pcap` | `capture_pcap.sh lenh`: N0, 20 lệnh hold QoS 1 (PX4 SITL); khứ hồi do commander đo ở `5_lenh_qos1_N0_cmd_rtt.csv` | Khứ hồi 42 ms là Nagle trên socket của Gateway gặp delayed ACK của Broker (xem dưới) |
| `6_N1_nodelay.pcap` | `capture_pcap.sh nodelay`: N1, telemetry QoS 1, 30 s, Broker bật `set_tcp_nodelay true` | p95 200 ms còn lại là Nagle trên socket của Gateway (xem dưới) |

Ở N2, netem bỏ gói trước khi gói tới bridge, nên Wireshark không thấy gói gốc. Wireshark báo
"Previous segment not captured" tại chỗ thiếu, sau đó là Dup ACK, rồi gói được TCP truyền lại.

## Nagle và delayed ACK (file 5 và 6)

Gateway chạy trên máy, nối Broker từ cổng `172.28.0.1:37468` (file 5; gói CONNECT của Gateway nằm trước lúc bắt
nên không có trong file) và `172.28.0.1:58148` (file 6). Commander ở cổng `40492`.

**`5_lenh_qos1_N0.pcap`, lệnh đầu tiên (gói 96–105).** Bộ lọc: `tcp.port == 37468 || tcp.port == 40492`.

| Gói | Thời gian (so với gói 96) | Chiều | Nội dung |
|---|---|---|---|
| 96 | 0 ms | commander → Broker | PUBLISH `drone/001/cmd` (QoS 1) |
| 97 | 0 ms | Broker → Gateway | PUBLISH `drone/001/cmd` |
| 98 | 0 ms | Broker → commander | PUBACK cho lệnh |
| 100 | 0,1 ms | Gateway → Broker | PUBACK cho lệnh. Gói nhỏ chưa được ACK, nên Nagle giữ mọi gói nhỏ tiếp theo |
| 101 | 40,4 ms | commander → Broker | TCP ACK cho gói 98 (delayed ACK), không giữ gì lại |
| 102 | 41,4 ms | Broker → Gateway | TCP ACK cho gói 100 (delayed ACK) |
| 103 | 41,4 ms | Gateway → Broker | PUBLISH `drone/001/cmd/ack`: lên dây ngay khi gói 102 tới. Theo `ack_ms` trong JSON, tin này đã nằm chờ trong Gateway khoảng 27 ms |
| 105 | 41,6 ms | Broker → commander | Broker chuyển tin ack ngay |

Trên cả 20 lệnh: Broker ACK gói PUBACK của Gateway sau 40,1–41,3 ms. Tin ack của Gateway lên dây 0,0 ms sau ACK
đó, sau khi chờ 19–36 ms. Broker chuyển tiếp mất 0,1–1,0 ms. Như vậy giả thuyết cũ (Nagle giữ gói ở chặng
Broker → commander) **không đúng**. Nagle và delayed ACK đúng là nguyên nhân, nhưng gói bị giữ nằm ở chặng Gateway → Broker.

**`6_N1_nodelay.pcap`.** Bộ lọc: `tcp.port == 58148 && mqtt`. Gói 789: Gateway gửi `position` ngay. Ba tin
`altitude`, `velocity`, `battery` cùng `timestamp_ms` chờ tới gói 792 (PUBACK + ACK của Broker, trễ 100 ms do
netem), rồi đi chung **một** gói TCP (gói 793). Phía Broker không giữ tin nào (Broker → Logger luôn đúng 100 ms
sau Gateway → Broker). Trên 392 tin sau 10 s đầu: trễ trong Gateway có trung vị 1 ms, p95 101 ms; tổng trễ có
trung vị 101 ms, p95 201 ms.

Mở file bằng Wireshark như các file trên. Muốn xem thời gian giữa hai gói thì chọn gói đầu,
bấm *Edit → Set/Unset Time Reference*, rồi đọc cột *Time* của gói sau.

## Ảnh đã chụp và cách đọc (dùng làm chú thích trong báo cáo)

| Ảnh | Gói | Đọc được gì |
|---|---|---|
| `1_connect.png` | 18, Gateway → Broker | MQTT v3.1.1; Client ID `gateway-01`; Keep Alive 10 s; Clean Session = 0 (phiên bền); Will Flag = 1, Will QoS 1, Will Retain = 1, Will Topic `drone/001/status`, Will Message `{"drone_id":"001","state":"offline"}`; tài khoản `drone` / `drone123` gửi dạng chữ thường |
| `2_connack.png` | 20, Broker → Gateway | Return Code 0 (Connection Accepted); **Session Present = 1**, nghĩa là Broker còn giữ phiên cũ của `gateway-01` vì Clean Session = 0 |
| `3_publish_puback_qos1.png` | 24 → 27 | Một tin QoS 1 đi qua **hai chặng**, mỗi chặng có PUBACK riêng: Gateway → Broker (gói 24, id=2) được Broker trả PUBACK (gói 26); Broker → Logger (gói 25, id=2) được Logger trả PUBACK (gói 27). Message Identifier của mỗi chặng do bên gửi chặng đó tự đặt |
| `4_publish_qos0.png` | 22 | QoS 0: không có Message Identifier, không có PUBACK. Gói "Publish Ack (id=2)" ở gói 32 trong danh sách là của tin `status` (QoS 1), không phải của telemetry |
| `5_tcp_retransmission.png` | 314, 316, 326–328 | Ở N2 (mất 5 %): "Previous segment not captured" (gói bị netem bỏ), Dup ACK từ Logger, rồi "TCP Retransmission" và "Out-Of-Order" khi TCP gửi lại. Gói 12 và 15 cho thấy cả gói bắt tay SYN cũng bị mất và gửi lại. Dòng "RTO … 0,000133 s" **không phải RTO thật**: vì gói gốc không bắt được, Wireshark tính từ gói 314 ngay trước. RTO thật của Linux khoảng 200 ms |
| `6_last_will.png` | 255, Broker → Logger | Gateway bị `kill -9` nên không gửi DISCONNECT. Broker thấy kết nối đứt, tự phát Will Message `{"state":"offline"}` lên `drone/001/status` (QoS 1). Cờ Retain trên gói này = 0 là đúng chuẩn MQTT 3.1.1: Broker gửi tin cho subscriber **đang online** thì xoá cờ Retain, nhưng vẫn **lưu** tin làm retained cho subscriber đến sau (xem gói 11 của `1_qos1.pcap`) |

## Hướng dẫn chụp lại

Mở file bằng Wireshark, gõ bộ lọc vào ô *Display filter*, bấm vào gói rồi mở khung *MQ Telemetry Transport*
ở phần chi tiết bên dưới. Lưu ảnh vào thư mục này, đặt tên như cột cuối.

| # | File | Bộ lọc | Gói cần bấm | Mở rộng / chú ý | Tên ảnh |
|---|---|---|---|---|---|
| 1 | `1_qos1.pcap` | `mqtt.msgtype == 1 \|\| mqtt.msgtype == 2` | gói 18 (CONNECT của Gateway; gói 4 là của Logger, không có Will) | Client ID, Keep Alive = 10, cờ Clean Session = 0, Will Topic / Will Message, User Name | `1_connect.png` |
| 2 | `1_qos1.pcap` | như trên | gói 20 (CONNACK trả cho Gateway) | Return Code = 0 (Connection Accepted), Session Present | `2_connack.png` |
| 3 | `1_qos1.pcap` | `mqtt.msgtype == 3 \|\| mqtt.msgtype == 4` | gói 25 và PUBACK ngay sau nó | QoS = 1, Message Identifier của PUBLISH trùng với PUBACK, nội dung JSON | `3_publish_puback_qos1.png` |
| 4 | `2_qos0.pcap` | `mqtt.msgtype == 3 \|\| mqtt.msgtype == 4` | gói 22 | QoS = 0, không có Message Identifier, không có PUBACK theo sau | `4_publish_qos0.png` |
| 5 | `3_mat_goi_N2.pcap` | `tcp.analysis.flags` | gói 316 (hoặc 328, 352) | dòng "TCP Retransmission" / "Previous segment not captured" / "Dup ACK" | `5_tcp_retransmission.png` |
| 6 | `4_last_will.pcap` | `mqtt.msgtype == 3 && frame contains "offline"` | gói 255 | Broker gửi Last Will `{"state":"offline"}` sau khi Gateway bị kill -9 | `6_last_will.png` |
