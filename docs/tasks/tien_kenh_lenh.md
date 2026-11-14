# Giao việc: Tiên – Kênh lệnh trên App và thí nghiệm H

Phần Gateway của kênh lệnh đã xong (Chiến, nhánh `feature/command-channel`). Việc của Tiên:
**(1)** thêm nút điều khiển vào App, **(2)** chạy thí nghiệm H và lưu số liệu.
Đọc trước `docs/command_channel.md` (giao thức lệnh/ack, 4 kiểm tra an toàn).

Chạy thử Gateway trên máy Linux:

```bash
cmake -B build && cmake --build build -j
./scripts/setup_broker.sh
./build/gateway/gateway --source fake --out /tmp/cmd     # drone giả lập, đang hover 25,5 m
```

---

## 1. App: nút điều khiển

### Cần làm

☐ Subscribe thêm `drone/001/cmd/ack` (QoS 1) cùng lúc với các topic khác trong `connect()`.

☐ Thêm một hàm gửi lệnh. Mỗi lệnh là JSON publish lên `drone/001/cmd`, **QoS 1, không retained**:

```dart
int _cmdId = DateTime.now().millisecondsSinceEpoch;   // tăng dần, không trùng giữa các lần mở app
final String operatorName = 'app-tien';               // trường "from"

void sendCommand(String action, {double? alt, double? lat, double? lon}) {
  final cmd = {
    'cmd_id': ++_cmdId,
    'from': operatorName,
    'action': action,
    'sent_ms': DateTime.now().millisecondsSinceEpoch,
    'ttl_ms': 3000,
    if (alt != null) 'alt': alt,
    if (lat != null) 'lat': lat,
    if (lon != null) 'lon': lon,
  };
  final b = MqttClientPayloadBuilder()..addString(jsonEncode(cmd));
  client!.publishMessage('drone/001/cmd', MqttQos.atLeastOnce, b.payload!, retain: false);
  // ghi lại thời điểm gửi theo cmd_id để tính thời gian khứ hồi khi nhận ack
}
```

☐ Các nút: **Cất cánh** (`takeoff`, ô nhập độ cao, mặc định 10 m), **Hạ cánh** (`land`),
**Quay về** (`rtl`), **Đứng yên** (`hold`), **Bay tới** (`goto`, hai ô nhập lat/lon; nút đặt sẵn
`47.3980, 8.5460` để demo), **Nhả quyền** (`release`).

☐ Trong `onMessage`: tin trên `drone/001/cmd/ack` có `"from" == operatorName` thì hiện kết quả lệnh gần
nhất: `accepted` (xanh) hoặc `rejected` + `reason` (đỏ), kèm **thời gian khứ hồi** (giờ nhận ack − giờ
gửi). Lưu ý: hàm hiện tại lấy `m.topic.split('/').last`, tin ack sẽ ra `ack`, cần tách riêng trước khi
xét `status` và 4 topic số liệu.

☐ Gửi lệnh mà **3 giây** không thấy ack: hiện "Không có phản hồi". Khoá các nút khi mất kết nối, giống
cách đang khoá ô tích topic.

☐ Hiện người đang giữ quyền điều khiển (trường `owner` trong ack).

### Ba điều không được làm

- **Không gửi lệnh với `retain: true`.** Gateway sẽ từ chối; nếu lỡ gửi, xoá bằng
  `mosquitto_pub -h 172.28.0.10 -u drone -P drone123 -r -n -t drone/001/cmd`.
- **Không tự gửi lại lệnh** khi chưa có ack. Bấm lại là một lệnh mới (cmd_id mới).
- **Không dùng lại `cmd_id`.** Cùng `from` + `cmd_id` sẽ bị từ chối `duplicate`.

### Xong khi

- Bấm **Hạ cánh**: ack `accepted`, độ cao trên app giảm dần về 0 (khoảng 13 giây).
- Bấm **Cất cánh** (15 m): độ cao tăng tới 15 m. Bấm **Cất cánh** lần nữa khi đang bay:
  `rejected: invalid_state`.
- Bấm **Bay tới**: vị trí và tốc độ thay đổi; tới nơi tốc độ về gần 0.
- Bấm **Quay về**: drone bay về `47.3977, 8.5456` rồi tự hạ cánh.
- Chạy cùng lúc `./build/commander/commander --action hold --from op2` trong khi App vừa gửi lệnh:
  một bên nhận `not_owner`, App hiện đúng người giữ quyền.
- Điện thoại thật: đồng hồ để **tự động đồng bộ**, nếu không lệnh sẽ bị `expired` (kiểm tra 2 so giờ
  máy gửi với giờ Gateway).
- Ảnh chụp màn hình từng trường hợp trên, lưu `data/H/app/`.

---

## 2. Thí nghiệm H

Chạy trên máy Linux có Broker, Gateway và `commander` **cùng một máy** (chung đồng hồ). Mỗi lượt lưu
một thư mục `data/H/<tên>/`.

### H1 – Thời gian khứ hồi lệnh theo điều kiện mạng

4 điều kiện (N0–N3) × QoS 0/1 × 3 lần = 24 lượt, mỗi lượt khoảng 1 phút. Một lượt:

```bash
D=data/H/N2_qos1_run1
./build/gateway/gateway --source fake --out $D &         # Terminal 1, hoặc chạy nền như dòng này
./scripts/netem.sh N2
./build/commander/commander --action hold --count 300 --interval-ms 200 --qos 1 \
    --scenario N2 --run 1 --out $D
./scripts/netem.sh N0
kill -INT %1                                              # dừng Gateway
```

Kết quả: `$D/cmd_rtt.csv` (mỗi lệnh một dòng: `rtt_ms`, `down_ms`, `up_ms`, `result`, `acks`) và
`$D/cmd_log.csv` của Gateway. Cần báo cáo: trung vị, p95 của `rtt_ms`; số lệnh `timeout`.

**Lưu ý:** ngay ở N0 (mạng bình thường) khứ hồi đã khoảng **40–65 ms**. Đây **không phải lỗi**: Chiến đã
xác định là do thuật toán Nagle của TCP gặp delayed ACK (Paho và Mosquitto mặc định không tắt Nagle).
Đây là một kết quả để viết vào báo cáo, không sửa cấu hình khi đang đo.

### H2 – Lệnh tới muộn (cảnh demo chính)

```bash
./build/gateway/gateway --source fake --out data/H/h2_on      # chạy 3 giây rồi Ctrl+C
./build/commander/commander --action goto --lat 47.3980 --lon 8.5460 --wait-ms 1000
#   (Gateway đang tắt nên commander báo timeout; Broker giữ lệnh lại)
#   đợi 10 giây
./build/gateway/gateway --source fake --out data/H/h2_on      # bật lại: lệnh bị expired, drone đứng yên
```

Làm lại y hệt với `--cmd-safety off` ở cả hai lần bật Gateway, thư mục `data/H/h2_off`: lệnh được
chấp nhận sau ~10 giây, drone bay đi. Mỗi trường hợp 3 lần. Nên quay màn hình App ở cả hai trường hợp.

Đã thử sơ bộ: lệnh bị giữ 9,1 s → bật kiểm tra thì `rejected: expired`, tắt kiểm tra thì drone bay 5 m/s.

### H3 – Lệnh trùng khi Broker khởi động lại (chưa thử, nếu số liệu lạ thì báo Chiến)

```bash
./build/gateway/gateway --source fake --out data/H/h3_run1 &
./build/commander/commander --action hold --count 100 --interval-ms 200 --qos 1 --out data/H/h3_run1 &
sleep 5; docker stop broker; sleep 5; docker start broker
```

Đếm trong `cmd_log.csv` của Gateway: số dòng `dup_flag = 1`, số lệnh `duplicate`, số lệnh `expired`.
Chạy 3 lần.

### Xong khi

- Đủ 24 thư mục H1, 6 thư mục H2, 3 thư mục H3 trong `data/H/`.
- Bảng: trung vị, p95 khứ hồi theo N0–N3 và QoS; số lệnh expired, duplicate.
- Video ngắn H2 (bật và tắt kiểm tra).
- Đẩy lên nhánh `data/thi-nghiem-H`, mở PR, chọn Chiến review.

## Phần báo cáo của Tiên (bổ sung)

Mục "Kênh lệnh": sơ đồ hai chiều telemetry/lệnh, bảng 4 nguy cơ và cách chặn (lấy từ
`docs/command_channel.md`), kết quả H1–H3, ảnh App.
