# PIDパース機能チェックツール (DebugApp) 仕様書

## 1. 概要
本ドキュメントは、RP2040デバイスに実装されたPID（Physical Interface Device）プロトコルのパース機能が正しく動作するかを検証するための、PC側テストアプリおよび通信仕様を定義するものです。

## 2. 通信構成
テストアプリは以下の2つのフローを使用して検証を行います。

1.  **送信 (PC → Device)**: `USB HID Output Report` を使用して、検証用のPIDデータ（Report ID: 0x01, 0x05, 0x0D）を送信します。
2.  **演算・同期 (Device Core0 ↔ Core1)**: Core0 でパースされた命令を共有メモリ経由で Core1 へ渡し、Core1 で必要に応じてループバック処理を行い Core0 へ戻します。
3.  **受信 (Device → PC)**: 
    *   **USB CDC (Serial)**: デバイス側でのパース結果および Core1 への到達確認ログを受け取ります。
    *   **USB HID Input Report**: Core1 からループバックされた値を HID 入力として受信し、UI のモニターに表示します。

```mermaid
sequenceDiagram
    participant App as PC Test App (Python)
    participant C0 as Dev Core0
    participant SM as Shared Memory (Mutex)
    participant C1 as Dev Core1
    
    App->>C0: HID Output Report (FFB命令)
    Note over C0: PIDパース実行
    C0-->>App: Serial: [PID_DEBUG] ログ
    
    C0->>SM: FFB命令を書き込み
    SM->>C1: Core1が命令を読み出し
    Note over C1: ループバック演算 (TEST時)
    C1->>SM: HID入力データを書き込み
    SM->>C0: Core0が入力データを読み出し
    
    C0->>App: HID Input Report (ループバック値)
    Note over App: UIモニターで往復を確認
```

## 3. USB I/O 仕様

### 3.1. PCからの送信データ (HID Output Report)
各レポートの構造は埋め込み側の `hidwffb.h` に準拠します。

| Report ID | 名称 | 主要フィールド | サイズ (bytes) |
| :--- | :--- | :--- | :--- |
| **0x01** | Set Effect | Effect Type(1), Gain(2), etc. | 14 |
| **0x05** | Set Constant Force | Magnitude(2) | 4 |
| **0x0D** | Device Gain | Device Gain(1) | 3 |

> [!IMPORTANT]
> Report ID 0x01 では、`Effect Type = 0x26` (Constant Force) の場合のみ、パース処理が行われます。

### 3.2. デバイスからの返却データ (Serial Debug Log)
デバイスはレポートを受信・パースした後、以下の形式でシリアル出力を行います。

*   **ID 0x01 受信時**:
    `[PID_DEBUG] ID:0x01, Type:Constant, Mag:<gain_value>, Gain:<device_gain_value>`
*   **ID 0x05 受信時**:
    `[PID_DEBUG] ID:0x05, Mag:<magnitude_value>`
*   **ID 0x0D 受信時**:
    `[PID_DEBUG] ID:0x0D, G:<gain_value>`

## 4. テストケース案

### 4.1. 正常系テスト
- **Report 0x01**: Effect Typeを0x26に設定し、Gainを 0, 16384, 32767 と変化させて、正しく認識されるか。
- **Report 0x05**: Magnitudeを -32767, 0, 32767 と変化させて、符号を含めて正しく認識されるか。
- **Report 0x0D**: Device Gainを 0, 128, 255 と変化させて、正しく認識されるか。

### 4.2. 異常系・境界値テスト
- **未対応ID**: 定義されていないパケットを送った際、デバイスがフリーズせず無視されるか。
- **範囲外の値**: 8bitフィールドに 256 以上の値を入れようとした場合や、不正な Effect Type (0x26以外) を送った際の挙動。
- **データ長不足**: Report ID に続くデータが構造体サイズに満たない場合に、パース処理を安全にスキップするか。

## 5. PCアプリ実装要件 (Python)
- **GUIライブラリ**: `customtkinter` または `tkinter` (要相談、モダンなUIにはCustomTkinterを推奨)。
- **HIDライブラリ**: `hidapi` (`pyusb` よりも Windows での動作が安定するため)。
- **シリアルライブラリ**: `pyserial`。
- **機能**:
    - COMポートおよびHIDデバイスの自動/手動選択。
    - 各Report IDに対応した入力フィールド（スライダー/数値入力）。
    - 受信ログ表示エリアおよび自動OK/NG判定表示。

## 6. マルチコア同期の検証 (コールバックテスト)

共有メモリを通じた Core 間同期の整合性を検証するため、以下の挙動が実装されています。

### 6.1. 挙動仕様
- **トリガー**: PC から PID レポート (0x01, 0x05, 0x0D 等) を受信した瞬間。
- **有効時間**: 受信から 5 秒間 (`isCoolBackTest` フラグが有効)。
- **ループバック内容**:
    - `Steer` (Z軸) <- `Magnitude` (0x05)
    - `Accel` (Rx軸) <- `Gain` (0x01)
    - `Brake` (Ry軸) <- `Device Gain` (0x0D)
- **確認方法**: 
    1. PC ツールで値を送信。
    2. デバイスのシリアルログで `[CORE1_DEBUG]` が出力されることを確認（Core1 への到達確認）。
    3. 5 秒間、PC ツールの `HID Input Monitor` の各バーが送信値に同期して動くことを確認。

### 6.2. デバッグログ形式
- `[CORE1_DEBUG] Mag:<value>, CoolBack:<0/1>`
    - Core1 が共有メモリから読み取った値を出力します。Core0 が受信してから Core1 が認識するまでの導通チェックに使用します。
