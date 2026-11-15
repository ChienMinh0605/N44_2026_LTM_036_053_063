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

Ở N2, netem bỏ gói trước khi gói tới bridge, nên Wireshark không thấy gói gốc. Wireshark báo
"Previous segment not captured" tại chỗ thiếu, sau đó là Dup ACK, rồi gói được TCP truyền lại.

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
