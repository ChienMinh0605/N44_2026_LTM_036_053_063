# Dữ liệu thí nghiệm

Mỗi lần chạy một thư mục riêng, đặt tên theo `<điều kiện>_qos<0|1>_run<số>`:

```
data/
├── E/N2_qos1_run3/     # sent.csv, events.csv, logger.csv, ping.txt
├── C/buffer-off_run1/
├── F/qos0_run2/
├── pcap/               # file Wireshark (.pcapng) và ảnh chụp
└── plots/              # bảng và biểu đồ do scripts/analyze.py sinh ra
```

## Cột của từng file

- **sent.csv** (Gateway): `boot_id, topic, seq, qos, timestamp_ms, status`, trong đó status là `sent` / `buffered` / `dropped`
- **events.csv** (Gateway): `time_ms, event`, trong đó event là `connection_lost` / `reconnect_attempt` / `connected`
- **logger.csv** (Logger): `recv_time_ms, topic, boot_id, seq, timestamp_ms, latency_ms, qos, scenario, run`
- **ping.txt**: kết quả `ping -c 50 172.28.0.10` ngay trước lần chạy
