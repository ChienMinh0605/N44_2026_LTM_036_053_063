# Dữ liệu thí nghiệm

Mỗi lần chạy một thư mục riêng, đặt tên theo `<điều kiện>_qos<0|1>_run<số>`:

```
data/
├── E/N2_qos1_run3/     # sent.csv, events.csv, logger.csv, ping.txt
├── C/buffer-off_run1/
├── F/qos0_run2/
├── pcap/               # file Wireshark (.pcapng) và ảnh chụp
└── plots/              # bảng và biểu đồ do build/scripts/b_analyze sinh ra
```

## Cột của từng file

- **sent.csv** (Gateway): `boot_id, topic, seq, qos, timestamp_ms, status`, trong đó status là `sent` / `buffered` / `dropped`
- **events.csv** (Gateway) và **logger_events.csv** (Logger): `time_ms, client_id, event, detail`, trong đó event là `connect_attempt` / `connected` / `connection_lost` / `connect_failed` / `disconnected`
- **logger.csv** (Logger): `recv_time_ms, topic, boot_id, seq, timestamp_ms, latency_ms, qos, scenario, run`
- **ping.txt**: kết quả `ping -c 50 172.28.0.10` ngay trước lần chạy
