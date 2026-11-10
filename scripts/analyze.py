"""Phân tích số liệu thí nghiệm (Nhật viết tiếp).

Đầu vào mỗi lần chạy (xem data/README.md):
  - sent.csv của Gateway: boot_id, topic, seq, qos, timestamp_ms, trạng thái
  - CSV của Logger: recv_time_ms, topic, boot_id, seq, timestamp_ms, latency_ms, qos, scenario, run

Cửa sổ tính: bỏ 10 giây đầu, chỉ tính tin có timestamp_ms trong khoảng còn lại;
sent.csv lọc theo đúng khoảng đó.

Chỉ số (mục 5 của kế hoạch):
  - Tỷ lệ nhận   = số bộ (boot_id, topic, seq) khác nhau Logger nhận / số dòng sent.csv
  - Số tin trùng = số bộ (boot_id, topic, seq) xuất hiện nhiều hơn 1 lần
  - Độ trễ       = recv_time_ms - timestamp_ms: trung vị, p95, đường CDF
  - Mỗi chỉ số: trung bình ± độ lệch chuẩn qua 3 lần chạy

Cần: pip install pandas matplotlib
"""


def main() -> None:
    # TODO: đọc các thư mục trong data/E, tính chỉ số, xuất bảng và biểu đồ vào data/plots
    raise SystemExit("TODO: chưa viết xong")


if __name__ == "__main__":
    main()
