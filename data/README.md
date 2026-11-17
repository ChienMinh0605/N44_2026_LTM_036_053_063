# Dữ liệu thí nghiệm

Mỗi lần chạy một thư mục riêng, đặt tên theo `<điều kiện>_qos<0|1>_run<số>`:

```
data/
├── E/N2_qos1_run3/     # sent.csv, events.csv, logger.csv, ping.txt
├── C/buffer-off_run1/  # scripts/run_CF.sh c; tổng hợp: data/summary_CF.md
├── F/qos0_run2/        # scripts/run_CF.sh f
├── pcap/               # file Wireshark (.pcapng) và ảnh chụp
└── plots/              # bảng và biểu đồ do build/scripts/b_analyze sinh ra
```

## Cột của từng file

- **sent.csv** (Gateway): `boot_id, topic, seq, qos, timestamp_ms, status`, trong đó status là `sent` / `buffered` / `dropped`
- **events.csv** (Gateway) và **logger_events.csv** (Logger): `time_ms, client_id, event, detail`, trong đó event là `connect_attempt` / `connected` / `connection_lost` / `connect_failed` / `disconnected`
- **logger.csv** (Logger): `recv_time_ms, topic, boot_id, seq, timestamp_ms, latency_ms, qos, scenario, run`
- **ping.txt**: kết quả `ping -c 50 172.28.0.10` ngay trước lần chạy
- **off_ms.txt / on_ms.txt** (C, F): Unix ms lúc tắt và lúc bật lại Broker (C) hoặc Logger (F)
- **scenario.txt**: tham số của lượt chạy (QoS, bộ đệm, thời lượng, lúc gây sự cố)
- **hover.txt** (E, C, F, H): độ cao drone PX4 SITL trước lượt đo, do `scripts/hover.sh` ghi (tự cất cánh nếu drone
  đang ở dưới đất)
- **C/reconnect_strace/**: thời điểm Gateway thử kết nối lại khi Broker tắt 15 s, đo riêng bằng `strace`

Số liệu C, E, F, H đo bằng PX4 SITL v1.17.0 ngày 17/11/2026 (ảnh A, B, D và `H/app` chưa chụp lại). Bộ số cũ đo
bằng nguồn dữ liệu giả còn trong git ở commit `5dd7169`.
