# PIDパース機能チェックツール 完了報告

PID （Physical Interface Device）プロトコルのパース機能を検証するための環境が整いました。

## 1. 実施内容

### 組み込み側 (RP2040)
- **HID記述子の更新**: PCアプリから Report ID 0x01, 0x05, 0x0D を Output Report として送信できるよう、記述子を修正しました。
- **パース処理の連携**: 受信した各レポートを `PID_ParseReport` へ転送するようコールバックを修正。
- **デバッグ出力の強化**: `main.cpp` のシリアル出力をアプリでの解析に適した形式 (`[PID_DEBUG] ...`) に統一しました。

### PC側 (Python App)
- **GUIツール作成**: [pid_tester.py](file:///d:/PlatformIO_Project/RPP2040_USB_Gamepad/tools/python/pid_tester.py) を作成しました。
  - スライダー操作によるリアルタイムな値の送信が可能です。
  - デバイスからのシリアルログを並行してキャプチャし、表示します。

## 2. 使用方法

### 事前準備
Python環境で以下のライブラリをインストールしてください。
```bash
pip install customtkinter hidapi pyserial
```

### 実行手順
1. プラットフォームIOから最新 host のコードをデバイスに書き込みます。
2. アプリを起動します：
   ```bash
   python tools/python/pid_tester.pyw
   ```
3. 右側のパネルで **Serial Port** と **HID Device** (Gamepad等) を選択して `Connect` を押します。
4. 各種スライダーを動かし、`Send Report` ボタンを押すと、左側のログ領域にデバイスからの応答が表示されます。

## 3. 期待される結果
- **Set Effect**: `Gain` を変えて送信すると、デバイスログの `Mag` と `Gain` が正しく更新されること。
- **Set Constant Force**: `Magnitude` を変えて送信すると、デバイスログの `Mag` が正しく更新されること。
- **Device Gain**: `Device Gain` を変えて送信すると、デバイスログの `G` が更新されること。

> [!NOTE]
> デバイス側のログに `[PID_DEBUG]` と表示されている行が、PCアプリからのコマンドを正しくパースした結果です。
