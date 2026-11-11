# PX4 MQTT – Thu thập và phân phối dữ liệu drone qua MQTT

Đồ án môn Lập trình mạng. Drone ảo (PX4 SITL + Gazebo) gửi telemetry qua MAVLink/UDP,
Gateway C++ đổi sang JSON và publish lên Mosquitto; App Flutter hiển thị, Logger C++ ghi CSV
để đo đạc QoS và khả năng phục hồi khi mất kết nối.

```
PX4 SITL ─MAVLink/UDP─▶ Gateway ─MQTT/TCP─▶ Mosquitto ─▶ App Flutter / Logger
```

Kế hoạch chi tiết (quy ước topic, JSON, thí nghiệm): [docs/Ke_hoach_lam_viec_PX4_MQTT.docx](docs/Ke_hoach_lam_viec_PX4_MQTT.docx)

## Cấu trúc

| Thư mục | Nội dung | Phụ trách |
|---|---|---|
| `common/` | Hằng số chung (`config.hpp`), định dạng JSON (`message.hpp`), tham số dòng lệnh, phần MQTT dùng chung | Chiến |
| `gateway/` | PX4 → JSON → MQTT | Nửa MQTT: Chiến · Nửa MAVSDK: người có máy chạy PX4 |
| `logger/` | Subscribe và ghi CSV | Chiến |
| `app/` | App Flutter | Tiên |
| `scripts/` | Dựng Broker, netem, chạy thí nghiệm, phân tích | Chiến (Broker, netem), Nhật (thí nghiệm, phân tích) |
| `mosquitto/` | Cấu hình Broker | Chiến |
| `data/` | CSV, PCAP, biểu đồ kết quả | Nhật |
| `docs/` | Kế hoạch, báo cáo, slide | Cả nhóm |

## Cài đặt (Ubuntu 22.04 / WSL2)

Cả 3 máy:

```bash
sudo apt install build-essential cmake git iproute2 nlohmann-json3-dev libpaho-mqtt-dev libpaho-mqttpp-dev libssl-dev mosquitto-clients docker.io
sudo usermod -aG docker $USER   # đăng xuất rồi vào lại để dùng docker không cần sudo
```

Nếu `apt` không có `libpaho-mqttpp-dev`, build Paho từ mã nguồn theo hướng dẫn ở
github.com/eclipse-paho/paho.mqtt.cpp.

Chỉ máy chạy drone: cài PX4 + Gazebo, QGroundControl, và MAVSDK C++ (gói `.deb` cho
Ubuntu 22.04 trên trang Releases của github.com/mavlink/MAVSDK, cài bằng `sudo dpkg -i`).

## Build

```bash
cmake -B build
cmake --build build -j
```

Máy không cài MAVSDK vẫn build được Gateway, chỉ chạy được `--source fake` (dữ liệu giả,
đủ cho mọi thí nghiệm). Máy có MAVSDK thì chạy được thêm `--source px4`.

```bash
./build/gateway/gateway --source fake --qos 1 --out data/thu   # chạy thử, Ctrl+C để dừng
```

## Chạy

```bash
./scripts/setup_broker.sh            # dựng Broker 172.28.0.10:1883, tài khoản drone/drone123
./build/logger/logger --qos 1
./build/gateway/gateway --source px4 --qos 1   # hoặc --source fake nếu không có PX4
./scripts/netem.sh N1                # đặt điều kiện mạng: N0 | N1 | N2 | N3 | show
```

## Quy tắc làm việc với Git

1. Không push thẳng lên `main`. Mỗi việc một nhánh: `feature/logger`, `feature/gateway-mqtt`, `feature/app`...
2. Đầu buổi lấy code mới: `git checkout main && git pull`, rồi `git checkout -b feature/<tên-việc>`.
3. Làm xong một phần nhỏ thì commit, đẩy lên: `git push -u origin feature/<tên-việc>`.
4. Mở pull request trên GitHub, gán 1 bạn review, review xong mới merge vào `main`.
5. Muốn đổi quy ước chung (topic, JSON, IP, tài khoản) thì sửa `common/include/common/config.hpp`
   hoặc kế hoạch qua pull request, báo cả nhóm.
6. Không đẩy file build, `mosquitto/data/` và `mosquitto/config/passwd` (đã có trong `.gitignore`).
