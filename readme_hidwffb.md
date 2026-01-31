# hidwffb モジュール 取扱説明書

`hidwffb` (HID with Force Feedback) は、Raspberry Pi RP2040 で Force Feedback (FFB) 対応のゲームコントローラを構築するためのモジュールです。

## 1. モジュール概要

*   **16ビット高解像度軸**: Z軸 (Steer), Rx軸 (Accel), Ry軸 (Brake) の3軸。
*   **16個のデジタルボタン**: 標準的なゲームパッドとして認識されます。
*   **FFB 対応 (Output Report)**: PC からの FFB 制御データ（64バイト）を受信可能です。

## 2. 依存関係

本モジュールは以下のライブラリおよび設定を必要とします。

### ライブラリ
*   **Adafruit TinyUSB Library (@ ^3.1.0)**

### platformio.ini 設定
USBスタックとして TinyUSB を使用するため、以下の設定が必要です。

```ini
lib_deps = 
    adafruit/Adafruit TinyUSB Library @ ^3.1.0

build_flags = 
    -DUSE_TINYUSB
    -DARDUINO_USB_MODE=0
```

## 3. API リファレンス

すべての関数は `hidwffb.h` をインクルードして使用します。

### 初期化 (setup() 内で実行)

*   `void hidwffb_begin(uint8_t poll_interval_ms = 1)`
    *   HID デバイスを初期化し、USB スタックを開始します。
    *   `poll_interval_ms`: USB ポーリング周期（デフォルト 1ms = 1000Hz）。
*   `void hidwffb_wait_for_mount(void)`
    *   USB ホストにマウントされるまでブロッキングして待機します。

### 送受信 (loop() 内で実行)

*   `bool hidwffb_send_report(custom_gamepad_report_t *report)`
    *   コントローラの状態を PC へ送信します。
*   `bool hidwffb_ready(void)`
    *   デバイスが送信可能な状態（マウント済み・サスペンド解除済み）か確認します。
*   `bool hidwffb_get_ffb_data(uint8_t *buffer)`
    *   PC から届いた最新の FFB データ（64バイト）を取得します。
    *   `bool hidwffb_get_pid_debug_info(pid_debug_info_t *info)`
    *   パースされたPIDプロトコルの要約データを取得します。
    *   デバッグ出力用として `main.cpp` 等で利用されます。

### PIDパース関数

*   `void PID_ParseReport(uint8_t const *buffer, uint16_t bufsize)`
    *   生データを受け取って内部構造体を更新する純粋なパース関数です。
    *   特定のUSBスタックに依存せず、`stdint.h` の型のみを使用しています。

## 4. PID プロトコルのサポート

本モジュールは、USB PID (Physical Interface Device) プロトコルの一部をパースする機能を備えています。

### サポート対象
*   **Constant Force (Report ID: 0x01)**: `effectType` が `0x26` の場合に Constant Force として処理。
*   **Constant Force Magnitude (Report ID: 0x05)**: 効果の強度設定。
*   **Device Gain (Report ID: 0x0D)**: デバイス全体のゲイン設定。
*   **Effect Operation (Report ID: 0x0A)**: エフェクトの開始・停止（Start / Solo / Stop）制御。

## 5. テストツール (Tools)
 
 本プロジェクトには、動作検証用の Python アプリケーションが `tools/python/` に用意されています。
 
 ### PID Tester (`pid_tester.pyw`)
 *   **用途**: PCからFBBレポートを送信し、デバイス側のパース結果をシリアルログで確認します。
 *   **主要機能**: Constant Force (ID:0x01, 0x05), Device Gain (ID:0x0D), Effect Operation (ID:0x0A) の送信テスト。
 
 ### HID Tester (`hid_tester.pyw`)
 *   **用途**: デバイスから送信されるHID入力レポート（ステアリング、アクセル等）のリアルタイム表示。
 *   **機能**: シリアル経由でのループバックテスト機能も備えています。
 
 ## 6. デバッグログとPCアプリケーションとの連携

PC側（Python等）での自動照合を容易にするため、シリアルポートから特定のプリフィックスを持つログを出力できます。

### ログの有効化
`main.cpp` 内の以下のマクロ定義を有効にしてください。
```cpp
#define PID_DEBUG_ENABLE
```

### ログフォーマット仕様
PCアプリ側で `readline()` して正規表現などでパース可能な形式です。

1.  **Constant Force 受信時**:
    *   `[PID_DEBUG] ID:0x01, Type:Constant, Mag:16384, Gain:255`
    *   `Mag`: 強度（-32767 to 32767）
    *   `Gain`: 現在設定されているデバイスゲイン（0 to 255）

2.  **Device Gain 受信時**:
    *   `[PID_DEBUG] ID:0x0D, G:255`
    *   `G`: デバイスゲイン値（0 to 255）

3.  **Effect Operation 受信時**:
    *   `[PID_DEBUG] ID:0x0A, Index:1, Op:Start`
    *   `Index`: エフェクトブロックインデックス（1 to 40）
    *   `Op`: 操作内容（Start, Solo, Stop）

## 6. マルチコア構成時の注意点

RP2040 でマルチコア（Core0/Core1）を利用する場合、以下の点に注意してください。

### USB タスクの固定 (Core0)
Adafruit TinyUSB の制約上、**USB 通信および `hidwffb` の API（特に `begin` や `send_report`）は Core0 で呼び出すこと**を推奨します。

### 排他制御の必要性
もし FFB データの演算を Core1 で行い、入出力を Core0 で行う場合は、共有バッファへのアクセスに排他制御（Mutex や `critical_section`）を導入してください。

> [!WARNING]
> 現状の `hidwffb_get_ffb_data()` は内部で `memcpy` を行っていますが、モジュール内に同期機構は持っていません。Core0 の割り込み（USB受信）と Core1 からの読み出しが衝突する可能性があるため、上位層でセマフォ等のロックをかけることを検討してください。

## 7. 実装例

```cpp
#include "hidwffb.h"

// デバッグ有効化
#define PID_DEBUG_ENABLE

void setup() {
  hidwffb_begin(1);
  hidwffb_wait_for_mount();
}

void loop() {
  // HID送信処理...

  // PIDデバッグ情報の取得と表示
#ifdef PID_DEBUG_ENABLE
  pid_debug_info_t pid_info;
  if (hidwffb_get_pid_debug_info(&pid_info)) {
    if (pid_info.lastReportId == 0x01) {
      Serial.printf("[PID_DEBUG] ID:0x01, Type:Constant, Mag:%d, Gain:%d\n", 
                    pid_info.magnitude, pid_info.deviceGain);
    }
  }
#endif
}
```
