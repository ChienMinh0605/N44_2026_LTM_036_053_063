# App Flutter (bạn C)

Tạo project ngay trong thư mục này:

```bash
cd app
flutter create --project-name drone_monitor .
flutter pub add mqtt_client
```

Quy ước kết nối:

- Broker: IP của laptop, cổng 1883, MQTT 3.1.1, tài khoản `drone` / `drone123`
- Subscribe `drone/001/#` (màn hình chính) hoặc chỉ `drone/001/battery` (thí nghiệm B)
- Topic `drone/001/status` là retained: vừa kết nối là biết Gateway đang online hay offline
- Định dạng JSON: xem mục 2 của kế hoạch trong `docs/`

Nếu Docker chạy trong WSL2 mà điện thoại không kết nối được: bật `networkingMode=mirrored`
trong `.wslconfig` (hoặc dùng `netsh interface portproxy`) và mở cổng 1883 trên Windows Firewall.
Không được thì chạy app trên desktop hoặc emulator ngay trên laptop.
