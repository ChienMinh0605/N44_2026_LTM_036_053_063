import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';

void main() {
  // Khi Broker tắt đột ngột, thư viện mqtt_client ném SocketException
  // ("Connection reset by peer") mà không ai bắt. App vẫn chạy và vẫn tự kết nối
  // lại, nhưng log hiện dòng đỏ "Unhandled Exception" trông như app bị lỗi.
  // runZonedGuarded bắt các lỗi đó: lỗi mạng thì ghi một dòng ngắn,
  // lỗi khác vẫn in đầy đủ để không che mất lỗi thật.
  runZonedGuarded(
    () => runApp(const MaterialApp(home: HomePage())),
    (error, stack) {
      if (error is SocketException) {
        debugPrint('Mất kết nối Broker, đang thử lại: ${error.message}');
      } else {
        debugPrint('Lỗi chưa xử lý: $error\n$stack');
      }
    },
  );
}

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  // Ô nhập thông tin Broker
  final hostController = TextEditingController(text: '127.0.0.1');
  final userController = TextEditingController(text: 'drone');
  final passController = TextEditingController(text: 'drone123');

  MqttServerClient? client;
  bool connected = false; // đang kết nối được với Broker hay không
  String error = '';

  // Trạng thái Gateway (topic drone/001/status)
  String gateway = 'chưa rõ';
  String gatewayTime = '';

  // 4 topic số liệu: drone/001/position, drone/001/altitude, ...
  final labels = {
    'position': 'Vị trí',
    'altitude': 'Độ cao',
    'velocity': 'Tốc độ',
    'battery': 'Pin',
  };

  // Giá trị đang hiển thị của từng topic
  final values = {
    'position': '-',
    'altitude': '-',
    'velocity': '-',
    'battery': '-',
  };

  // Số tin đã nhận của từng topic (thí nghiệm B)
  final counts = {
    'position': 0,
    'altitude': 0,
    'velocity': 0,
    'battery': 0,
  };

  // Topic nào đang được chọn nhận (thí nghiệm B: chỉ tích Pin)
  final selected = {
    'position': true,
    'altitude': true,
    'velocity': true,
    'battery': true,
  };

  String dataTime = '';

  // Lấy giờ hiện tại dạng 15:04:05
  String timeNow() {
    final t = DateTime.now();
    String two(int n) => n.toString().padLeft(2, '0');
    return '${two(t.hour)}:${two(t.minute)}:${two(t.second)}';
  }

  Future<void> connect() async {
    final c = MqttServerClient(
        hostController.text, 'app-${DateTime.now().millisecondsSinceEpoch}');
    c.port = 1883;
    c.keepAlivePeriod = 20;
    c.setProtocolV311();
    c.autoReconnect = true; // mất mạng thì tự kết nối lại

    // Khi mất / có lại kết nối thì vẽ lại màn hình
    c.onAutoReconnect = () => setState(() => connected = false);
    c.onAutoReconnected = () => setState(() => connected = true);
    c.onDisconnected = () => setState(() => connected = false);

    try {
      await c.connect(userController.text, passController.text);
    } catch (e) {
      c.disconnect();
      setState(() => error = 'Không kết nối được: $e');
      return;
    }

    client = c;
    c.updates!.listen(onMessage);

    // status luôn nhận, còn 4 topic số liệu thì chỉ nhận topic đang được tích
    c.subscribe('drone/001/status', MqttQos.atLeastOnce);
    for (final name in labels.keys) {
      if (selected[name] == true) {
        c.subscribe('drone/001/$name', MqttQos.atLeastOnce);
      }
    }

    setState(() {
      connected = true;
      error = '';
    });
  }

  void disconnect() {
    client?.autoReconnect = false;
    client?.disconnect();
    setState(() {
      client = null;
      connected = false;
      gateway = 'chưa rõ';
    });
  }

  // Tích / bỏ tích một topic
  void selectTopic(String name, bool value) {
    setState(() {
      selected[name] = value;
      if (value) {
        client!.subscribe('drone/001/$name', MqttQos.atLeastOnce);
      } else {
        client!.unsubscribe('drone/001/$name');
        values[name] = '-';
      }
    });
  }

  // Đổi phần "value" trong JSON thành chữ để hiển thị
  String formatValue(String name, Map v) {
    if (name == 'position') {
      return '${v['lat'].toStringAsFixed(6)}, ${v['lon'].toStringAsFixed(6)}';
    }
    if (name == 'altitude') {
      return '${v['rel_m'].toStringAsFixed(1)} m';
    }
    if (name == 'velocity') {
      return '${v['ground_mps'].toStringAsFixed(1)} m/s';
    }
    if (name == 'battery') {
      return '${v['percent'].toStringAsFixed(1)} %   '
          '${v['voltage'].toStringAsFixed(2)} V';
    }
    return '$v';
  }

  // Nhận bản tin từ Broker
  void onMessage(List<MqttReceivedMessage<MqttMessage>> messages) {
    for (final m in messages) {
      final publish = m.payload as MqttPublishMessage;
      final text =
          MqttPublishPayload.bytesToStringAsString(publish.payload.message);

      Map<String, dynamic> data;
      try {
        data = jsonDecode(text);
      } catch (e) {
        continue; // bỏ qua bản tin không phải JSON
      }

      // Tên topic cuối cùng: drone/001/battery -> battery
      final name = m.topic.split('/').last;

      setState(() {
        if (name == 'status') {
          gateway = data['state'];
          gatewayTime = timeNow();
        } else if (counts.containsKey(name)) {
          // Không lọc theo ô tích: Broker gửi tin nào tới thì đếm tin đó.
          // Bỏ tích mà số đếm đứng yên => Broker đã thật sự ngừng gửi.
          counts[name] = counts[name]! + 1;
          values[name] = formatValue(name, data['value']);
          dataTime = timeNow();
        }
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Drone Monitor')),
      body: Padding(
        padding: const EdgeInsets.all(16),
        child: client == null ? loginView() : dataView(),
      ),
    );
  }

  // Màn hình nhập IP, user, mật khẩu
  Widget loginView() {
    return ListView(
      children: [
        TextField(
          controller: hostController,
          decoration: const InputDecoration(labelText: 'IP Broker'),
        ),
        TextField(
          controller: userController,
          decoration: const InputDecoration(labelText: 'User'),
        ),
        TextField(
          controller: passController,
          obscureText: true,
          decoration: const InputDecoration(labelText: 'Mật khẩu'),
        ),
        const SizedBox(height: 16),
        ElevatedButton(onPressed: connect, child: const Text('Kết nối')),
        const SizedBox(height: 8),
        Text(error, style: const TextStyle(color: Colors.red)),
      ],
    );
  }

  // Màn hình số liệu
  Widget dataView() {
    Color color = Colors.grey;
    if (gateway == 'online') color = Colors.green;
    if (gateway == 'offline') color = Colors.red;

    return ListView(
      children: [
        if (!connected)
          const Text('Mất kết nối Broker, đang thử lại...',
              style: TextStyle(color: Colors.orange)),
        Row(
          children: [
            Icon(Icons.circle, color: color),
            const SizedBox(width: 8),
            Text('Gateway: $gateway',
                style: TextStyle(fontSize: 20, color: color)),
          ],
        ),
        Text('Đổi trạng thái lúc: $gatewayTime'),
        const Divider(),
        const Text('Tích vào topic muốn nhận:'),
        for (final name in labels.keys)
          CheckboxListTile(
            title: Text('${labels[name]}: ${values[name]}',
                style: const TextStyle(fontSize: 18)),
            subtitle: Text('Đã nhận: ${counts[name]} tin'),
            value: selected[name],
            onChanged: (value) => selectTopic(name, value!),
          ),
        Text('Cập nhật lúc: $dataTime'),
        TextButton(
          onPressed: () => setState(() {
            for (final name in counts.keys) {
              counts[name] = 0;
            }
          }),
          child: const Text('Đặt lại bộ đếm'),
        ),
        const Divider(),
        ElevatedButton(onPressed: disconnect, child: const Text('Ngắt kết nối')),
      ],
    );
  }
}
