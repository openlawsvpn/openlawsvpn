import 'package:flutter/material.dart';
import 'package:file_picker/file_picker.dart';
import 'services/vpn_service.dart';

void main() {
  runApp(const OpenVPNApp());
}

class OpenVPNApp extends StatelessWidget {
  const OpenVPNApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'OpenLawsVPN',
      theme: ThemeData(
        brightness: Brightness.dark,
        primarySwatch: Colors.blue,
        useMaterial3: true,
      ),
      home: const DashboardPage(),
    );
  }
}

class DashboardPage extends StatefulWidget {
  const DashboardPage({super.key});

  @override
  State<DashboardPage> createState() => _DashboardPageState();
}

class _DashboardPageState extends State<DashboardPage> {
  final VpnService _vpnService = VpnService();
  final List<String> _logs = [];
  String? _configPath;
  final ScrollController _scrollController = ScrollController();

  @override
  void initState() {
    super.initState();
    _vpnService.logs.listen((log) {
      setState(() {
        _logs.add(log);
      });
      _scrollToBottom();
    });
  }

  void _scrollToBottom() {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (_scrollController.hasClients) {
        _scrollController.animateTo(
          _scrollController.position.maxScrollExtent,
          duration: const Duration(milliseconds: 200),
          curve: Curves.easeOut,
        );
      }
    });
  }

  Future<void> _pickConfig() async {
    FilePickerResult? result = await FilePicker.platform.pickFiles(
      type: FileType.custom,
      allowedExtensions: ['ovpn'],
    );

    if (result != null) {
      setState(() {
        _configPath = result.files.single.path;
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Dashboard'),
        actions: [
          IconButton(
            icon: const Icon(Icons.folder_open),
            onPressed: _vpnService.currentStatus == VpnStatus.disconnected
                ? _pickConfig
                : null,
          ),
        ],
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            _buildStatusCard(),
            const SizedBox(height: 16),
            _buildActionButtons(),
            const SizedBox(height: 16),
            const Text('Logs', style: TextStyle(fontWeight: FontWeight.bold)),
            const SizedBox(height: 8),
            Expanded(
              child: _buildLogConsole(),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildStatusCard() {
    return StreamBuilder<VpnStatus>(
      stream: _vpnService.status,
      initialData: VpnStatus.disconnected,
      builder: (context, snapshot) {
        final status = snapshot.data!;
        Color color;
        String text;
        IconData icon;

        switch (status) {
          case VpnStatus.disconnected:
            color = Colors.grey;
            text = 'Disconnected';
            icon = Icons.cloud_off;
            break;
          case VpnStatus.connecting:
            color = Colors.orange;
            text = 'Connecting...';
            icon = Icons.sync;
            break;
          case VpnStatus.authenticating:
            color = Colors.blue;
            text = 'Authenticating (SAML)...';
            icon = Icons.vpn_key;
            break;
          case VpnStatus.connected:
            color = Colors.green;
            text = 'Connected';
            icon = Icons.cloud_done;
            break;
        }

        return Card(
          child: Padding(
            padding: const EdgeInsets.all(20.0),
            child: Row(
              children: [
                Icon(icon, size: 48, color: color),
                const SizedBox(width: 20),
                Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(text, style: const TextStyle(fontSize: 24, fontWeight: FontWeight.bold)),
                    if (_configPath != null)
                      Text('Profile: ${_configPath!.split('/').last}', style: const TextStyle(color: Colors.white70)),
                  ],
                ),
              ],
            ),
          ),
        );
      },
    );
  }

  Widget _buildActionButtons() {
    return StreamBuilder<VpnStatus>(
      stream: _vpnService.status,
      initialData: VpnStatus.disconnected,
      builder: (context, snapshot) {
        final status = snapshot.data!;
        final isDisconnected = status == VpnStatus.disconnected;

        return Row(
          mainAxisAlignment: MainAxisAlignment.spaceEvenly,
          children: [
            ElevatedButton.icon(
              icon: const Icon(Icons.play_arrow),
              label: const Text('CONNECT'),
              style: ElevatedButton.styleFrom(
                backgroundColor: Colors.green,
                foregroundColor: Colors.white,
                padding: const EdgeInsets.symmetric(horizontal: 32, vertical: 16),
              ),
              onPressed: (isDisconnected && _configPath != null)
                  ? () => _vpnService.connect(_configPath!)
                  : null,
            ),
            ElevatedButton.icon(
              icon: const Icon(Icons.stop),
              label: const Text('DISCONNECT'),
              style: ElevatedButton.styleFrom(
                backgroundColor: Colors.red,
                foregroundColor: Colors.white,
                padding: const EdgeInsets.symmetric(horizontal: 32, vertical: 16),
              ),
              onPressed: !isDisconnected
                  ? () => _vpnService.disconnect()
                  : null,
            ),
          ],
        );
      },
    );
  }

  Widget _buildLogConsole() {
    return Container(
      padding: const EdgeInsets.all(8),
      decoration: BoxDecoration(
        color: Colors.black,
        borderRadius: BorderRadius.circular(4),
        border: Border.all(color: Colors.white24),
      ),
      child: ListView.builder(
        controller: _scrollController,
        itemCount: _logs.length,
        itemBuilder: (context, index) {
          return Text(
            _logs[index],
            style: const TextStyle(fontFamily: 'monospace', fontSize: 12),
          );
        },
      ),
    );
  }
}
