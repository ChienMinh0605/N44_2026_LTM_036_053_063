# PHẦN B — Công cụ thí nghiệm & phân tích số liệu (C++17)

## Phạm vi

Phần B theo `Ke_hoach_lam_viec_PX4_MQTT.docx`:

1. `scripts/fake_publisher.cpp`: publisher C++ độc lập cho C kiểm thử app, 4 topic × 5 Hz, JSON chung, `boot_id`, `seq`, `timestamp_ms`, status retained + Last Will.
2. `scripts/experiment_runner.cpp`: trình chạy thí nghiệm E, C++ chạy **Ubuntu/WSL2**; gọi `scripts/netem.sh` để cấu hình Linux `tc`. `scripts/run_experiment.sh` là wrapper mỏng.
3. Wireshark: bắt CONNECT/CONNACK, PUBLISH/PUBACK, TCP retransmission; xem hướng dẫn bên dưới.
4. `scripts/analyze.cpp`: đọc `sent.csv` + `logger.csv`, tính tỷ lệ nhận, trùng tin, median, p95, trung bình ± độ lệch chuẩn, xuất `per_run.csv`, `summary.csv`, `cdf.svg`, `receive_rate.svg`.
5. Nội dung cơ sở lý thuyết / báo cáo ở cuối file.

**Không chỉnh sửa `common/`, `gateway/`, `logger/`, `app/` của các thành viên.** File `CMakeLists.txt` ở root chỉ được thêm `add_subdirectory(scripts)`.

## A. Build trên Windows

Dùng **x64 Native Tools Command Prompt**, đã cài `vcpkg` ở `C:/dev/vcpkg` với `paho-mqttpp3:x64-windows`, `nlohmann-json:x64-windows`.

```cmd
cd /d "C:\Users\Administrator\Downloads\N44_2026_LTM_036_053_063-main"
cmake -S . -B build -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

Đường dẫn `cd` là ví dụ; dùng đúng đường dẫn repo thật sau khi giải nén. Chương trình B ở `build\scripts\Release\b_fake_publisher.exe`, `build\scripts\Release\b_analyze.exe`. Không có `b_experiment_runner.exe` vì tool này cần `fork`, `tc`, `nsenter` của Linux.

**Cấu hình Broker:** mã hiện tại của nhóm đặt Broker là `tcp://172.28.0.10:1883`, user `drone`, password `drone123` trong `common/include/common/config.hpp`. Không dùng MQTT Broker Windows 127.0.0.1 mà không đổi cấu hình này. Để kết nối Broker Windows chạy local, cần thống nhất với nhóm rồi thay Broker URL và cấu hình tài khoản (tránh commit thay đổi cấu hình chung không được duyệt).

```cmd
build\scripts\Release\b_fake_publisher.exe --qos 1 --duration 60
```

Mỗi 200 ms phát một chu kỳ 4 topic; gửi status `online` retained khi connected, `offline` retained khi thoát. Publisher này **chỉ để smoke test**, không tạo `sent.csv` và **không dùng làm nguồn đo E**.

## B. Build và chạy trên Ubuntu/WSL2

Cần `cmake`, `g++`, Paho C++, nlohmann-json, `iproute2`, `util-linux` (`nsenter`), `iputils-ping`, Docker Engine, kernel hỗ trợ netem. A chịu trách nhiệm Broker và Logger/Gateway.

```bash
sudo apt install -y build-essential cmake libpaho-mqtt-dev libpaho-mqttpp-dev nlohmann-json3-dev iproute2 iputils-ping util-linux
cmake -S . -B build
cmake --build build -j
```

### Chạy một kịch bản E

**Chỉ chạy trên máy Ubuntu/WSL2 nơi Broker 172.28.0.10 và Docker Engine hoạt động.** Chạy từ gốc repo:

```bash
./scripts/setup_broker.sh     # do A thực hiện, nếu Broker chưa được dựng
./scripts/run_experiment.sh N1 1 2
```

Tương đương:

```bash
./build/scripts/b_experiment_runner N1 1 2
```

Trình chạy sẽ tự thực hiện:

1. `netem.sh N0` kiểm tra Broker và reset, tự lấy PID mới.
2. Logger `--clean`: xóa phiên MQTT trước khi đặt netem.
3. `netem.sh N1`: áp dụng netem, `show`: xác nhận. Ping 50 gói và lưu `ping.txt`.
4. Logger chạy trước Gateway 3 giây (để subscribe). Gateway chạy `--source fake --qos 1 --buffer on --scenario N1 --run 2`.
5. Gateway chạy 180 giây, dừng bằng SIGINT, Logger chạy thêm 15 giây rồi dừng.
6. Reset N0 kể cả khi có lỗi. Lưu `sent.csv`, `events.csv`, `logger.csv`, `gateway.log`, `logger.log`, `ping.txt`, `netem*.txt`, `scenario.txt`.

Trình chạy **không ghi đè** dữ liệu đo hiện có trong thư mục kịch bản, để tránh pha trộn hai lần thử. Nếu muốn làm lại, di chuyển thư mục đo cũ ra ngoài trước. Bất kỳ lỗi cấu hình `sudo nsenter`, Broker, hoặc tool Linux đều làm lần chạy thất bại và cần sửa trước khi tin kết quả.

### Chạy đủ 24 lượt

```bash
./scripts/run_all_E.sh
```

Chạy QoS xen kẽ: run 1 (0 rồi 1), run 2 (1 rồi 0), run 3 (0 rồi 1) cho từng N0–N3; sau đó phân tích tự động ở chế độ `--strict`.

## C. Phân tích CSV bằng C++ (Windows hoặc Linux)

Khi A đưa `sent.csv` và `logger.csv` trong `data/E/N0_qos0_run1`, ...:

**Windows**:

```cmd
build\scripts\Release\b_analyze.exe --input data/E --out data/plots
```

**Linux**:

```bash
./build/scripts/b_analyze --input data/E --out data/plots --strict
```

`--strict` yêu cầu đủ 24 folder; không có `--strict` sẽ bỏ qua folder chưa có đủ CSV và cảnh báo nếu thiếu.

Các file:

- `per_run.csv`: chỉ số từng lượt.
- `summary.csv`: nhóm theo scenario/QoS, trung bình ± SD qua 3 lượt.
- `cdf.svg`: đồ thị CDF của độ trễ, gộp các lượt theo scenario/QoS.
- `receive_rate.svg`: tỷ lệ nhận trung bình theo điều kiện mạng và QoS.

**Quy tắc phân tích:** Loại 10 giây đầu từ `sent.csv`; Logger chỉ tính tin có định danh `(boot_id, topic, seq)` thuộc bộ gửi còn lại; bản tin nhận lặp chỉ được tính một lần vào tỷ lệ nhận nhưng đếm số lần bản tin dư. Độ trễ tính bằng `recv_time_ms - timestamp_ms`. Trung vị và p95 dùng nội suy trên số liệu đã sắp tăng; độ lệch chuẩn mẫu theo N−1. `sent.csv` phải là bản do Gateway tạo, không phải riêng Publisher B.

## D. Wireshark

- Tạo thư mục `data/pcap/` và chạy Wireshark ở interface có MQTT (`tcp.port == 1883`). Với Broker trong container có thể dùng `sudo nsenter -t "$(docker inspect -f '{{.State.Pid}}' broker)" -n tcpdump -i eth0 -s 0 -w /tmp/mqtt.pcap 'tcp port 1883'` rồi chép PCAP ra `data/pcap/` (nếu `tcpdump` được cài trên host).
- CONNECT/CONNACK: lọc `mqtt.msgtype == 1 || mqtt.msgtype == 2`. Bắt đầu capture **trước** khi client kết nối.
- PUBLISH/PUBACK: lọc `mqtt.msgtype == 3 || mqtt.msgtype == 4`; dùng QoS 1 để có PUBACK.
- TCP retransmission: `tcp.analysis.retransmission`; gói retransmission không đảm bảo xuất hiện ở mỗi lần đo; thử điều kiện N2/N3 và quan sát.
- Chụp hình packet details, nêu ý nghĩa cờ/fields. Không push PCAP chứa password MQTT thật: CONNECT MQTT không mã hóa có thể lộ thông tin xác thực.

## E. Dàn ý báo cáo B

1. MQTT: kiến trúc publisher/broker/subscriber, topic, MQTT 3.1.1, retain, Last Will, QoS 0/1, TCP retransmission.
2. Bộ thí nghiệm: Broker Docker IP cố định, `netem` trên eth0 Broker, **chỉ ảnh hưởng chiều Broker → client / PUBACK**, hover / fake source, mỗi topic 5 Hz, 4 topic, Logger cùng máy Gateway để đo độ trễ.
3. Thiết kế E: 2 QoS × 4 điều kiện N0–N3 × 3 lần = 24 lượt; 180 giây/lượt, Logger nhận thêm 15 giây, bỏ 10 giây đầu.
4. Chỉ số: tỷ lệ nhận duy nhất, số tin trùng, median, p95, CDF; thống kê trung bình ± SD 3 lượt.
5. Chèn `summary.csv`, `receive_rate.svg`, `cdf.svg`, hình chụp Wireshark; kết luận **chỉ sau khi chạy số liệu thật**. MQTT trên TCP có thể làm tăng độ trễ khi mạng mất gói thay vì mất bản tin ứng dụng trực tiếp.

## GitHub: chỉ push phần B

Repo nhóm có `main`, nên không push thẳng. Trong bản ZIP đầy đủ, có thể lấy **riêng các file được liệt kê** từ gói `PX4_MQTT_PART_B_ONLY.zip` để chép vào repo Git clone. Lưu ý `scripts/run_experiment.sh` và `scripts/analyze.py` của nhóm đang là TODO; bản mới thay run_experiment bằng launcher C++, đồng thời **xóa** `scripts/analyze.py` cũ vì đã chuyển hoàn toàn sang C++.

```bash
git switch main
git pull
git switch -c feature/experiment-tools-b
# copy file B vào repo đã clone, KHÔNG copy đè common/gateway/logger/app
git rm -f scripts/analyze.py   # nếu file TODO này có trong repo
git add CMakeLists.txt scripts/fake_publisher.cpp scripts/analyze.cpp scripts/experiment_runner.cpp scripts/CMakeLists.txt scripts/run_experiment.sh scripts/run_all_E.sh docs/PART_B_GUIDE.md
git commit -m "feat(part-b): C++ MQTT test tools and experiment analysis"
git push -u origin feature/experiment-tools-b
```

Sau đó mở Pull Request tới `main`, nhờ A review. Không đẩy `build/`, password, hoặc CSV thí nghiệm chưa làm thật.

## Kiểm tra trước khi nộp

- [ ] Windows build `b_fake_publisher` với Paho và CMake đã cài; gửi 4 topic + status tới Broker theo config nhóm.
- [ ] A hoàn thiện Broker/Gateway/Logger, chạy 1 lượt E trên Linux thành công.
- [ ] Có PCAP và ảnh CONNECT/CONNACK, PUBLISH/PUBACK, retransmission (nếu ghi được).
- [ ] Có đủ 24 lượt E với 24 cặp `sent.csv` + `logger.csv`, ping và log.
- [ ] `b_analyze --strict` sinh summary + 2 SVG, không lỗi.
- [ ] Đối chiếu biểu đồ với bảng CSV, viết nhận xét dựa trên số liệu thật.
