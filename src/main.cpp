/**
 * @file main.cpp
 * @brief RP2040 ゲームコントローラ (FFB対応) メインプログラム
 *
 * Core0: USB通信、入力読み取り、HID送信
 * Core1: FFB演算、モータドライバ制御 (将来追加)
 */

#include "hidwffb.h"
#include "util.h"
#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <SPI.h>

// --- ピン定義 ---
#define PIN_ACCEL 26      // A0 (GP26)
#define PIN_BRAKE 27      // A1 (GP27)
#define PIN_SHIFT_UP 25   // GP25
#define PIN_SHIFT_DOWN 24 // GP24
#define PIN_SPI_CS 17     // SPI CS (操舵軸センサ用)

// --- デバッグ設定 ---
// 原則、platformio.iniで定義する
// #define HID_INPUT_DEBUG_ENABLE ///< HID入力データをシリアル出力する

// --- 周期管理 ---
const uint32_t LOOP_INTERVAL_MS = 1;    ///< 1000Hz周期
const uint32_t LOOP_INTERVAL_LOOP1 = 1; ///< 1000Hz周期

IntervalTrigger_m loop_trigger(LOOP_INTERVAL_MS);     ///< Core0 1000Hz周期
IntervalTrigger_m loop1_trigger(LOOP_INTERVAL_LOOP1); ///< Core1 1000Hz周期

// --- タイマー管理 ---
OneShotTrigger_m cool_back_test_timer(5000); ///< 5秒のワンショットタイマー
#ifdef HID_INPUT_DEBUG_ENABLE
OneShotTrigger_m dummy_override_timer(5000); ///< ダミーデータ用5秒タイマー
#endif

// --- FFBデータ共有用 (Core0 <-> Core1) ---
uint8_t current_ffb_buf[HID_FFB_REPORT_SIZE];

void setup() {
  Serial.begin(115200);

  // I/O初期化
  pinMode(PIN_ACCEL, INPUT);
  pinMode(PIN_BRAKE, INPUT);
  pinMode(PIN_SHIFT_UP, INPUT_PULLUP);
  pinMode(PIN_SHIFT_DOWN, INPUT_PULLUP);
  pinMode(PIN_SPI_CS, OUTPUT);
  digitalWrite(PIN_SPI_CS, HIGH);

  // SPI初期化 (操舵軸センサ用)
  SPI.begin();

  // HIDモジュールの初期化 (1msポーリング)
  hidwffb_begin(LOOP_INTERVAL_MS);

  // USB接続待ち
  hidwffb_wait_for_mount();

  // 共有メモリ初期化
  ffb_shared_memory_init();

  Serial.println("System Refactored: HID Gamepad Ready (Core0)");
  loop_trigger.init();
}

void loop() {
  // 1ms周期で実行 (util.h の IntervalTrigger_m を使用)
  if (loop_trigger.hasExpired()) {

    if (hidwffb_ready()) {
#ifdef HID_INPUT_DEBUG_ENABLE
      static custom_gamepad_report_t dummy_report = {0, 0, 0, 0};

      // シリアル受信処理
      if (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        if (line.startsWith("HID:")) {
          int s_idx = line.indexOf('S'), a_idx = line.indexOf(",A"),
              b_idx = line.indexOf(",B"), btn_idx = line.indexOf(",BTN");
          if (s_idx != -1 && a_idx != -1 && b_idx != -1 && btn_idx != -1) {
            dummy_report.steer = line.substring(s_idx + 1, a_idx).toInt();
            dummy_report.accel = line.substring(a_idx + 2, b_idx).toInt();
            dummy_report.brake = line.substring(b_idx + 2, btn_idx).toInt();
            dummy_report.buttons = line.substring(btn_idx + 4).toInt();
            dummy_override_timer.start();
            Serial.println("[HID_DEBUG] Dummy Data Serial Received");
          }
        }
      }

      if (dummy_override_timer.isRunning()) {
        // ダミーデータ送信
        if (dummy_override_timer.hasExpired()) {
          // タイマー満了してもisRunningがfalseになるだけだが明示的に扱う場合はここで処理
        }
        hidwffb_send_report(&dummy_report);
      } else {
#endif
        // --- 物理入力読み取り (Core0内での処理は無効化に近い状態にする) ---
        // テスト時は Core1 のループバック値が共有メモリ経由で届く。
        // 物理入力を反映させたい場合は、ここで共有メモリを介して Core1
        // に渡すか、 あるいは Core1 側で物理入力を読み込む設計にする。
        // 今回はシンプルに、Core1が生成した値をCore0が送信する。
#ifdef HID_INPUT_DEBUG_ENABLE
      }
#endif
    }

    // --- FFBデータ更新チェックおよび共有 ---
    if (hidwffb_get_ffb_data(current_ffb_buf)) {
      // 受信データを検知したらタイマーを5秒にセット
      cool_back_test_timer.start();
    }

    // タイマー監視とフラグ更新
    bool is_cool_back_active = cool_back_test_timer.isRunning();
    if (is_cool_back_active && cool_back_test_timer.hasExpired()) {
      is_cool_back_active = false;
    }

    // --- PID解析結果の共有 ---
    pid_debug_info_t pid_info;
    if (hidwffb_get_pid_debug_info(&pid_info)) {
#ifdef PID_DEBUG_ENABLE
      if (pid_info.lastReportId == 0x01) {
        Serial.print("[PID_DEBUG] ID:0x01, Type:");
        Serial.print(pid_info.isConstantForce ? "Constant" : "Unknown");
        Serial.print(", Mag:");
        Serial.print(pid_info.magnitude);
        Serial.print(", Gain:");
        Serial.println(pid_info.deviceGain);
      } else if (pid_info.lastReportId == 0x05) {
        Serial.print("[PID_DEBUG] ID:0x05, Mag:");
        Serial.println(pid_info.magnitude);
      } else if (pid_info.lastReportId == 0x0A) {
        Serial.print("[PID_DEBUG] ID:0x0A, Op:");
        Serial.println(pid_info.operation);
      } else if (pid_info.lastReportId == 0x0D) {
        Serial.print("[PID_DEBUG] ID:0x0D, DeviceGain:");
        Serial.println(pid_info.deviceGain);
      }
#endif
      pid_info.updated = is_cool_back_active; // 共有メモリへ渡すフラグ
      ffb_core0_update_shared(&pid_info);
    } else {
      // PID受信がなくても定期的に共有メモリを更新（タイマー切れ反映のため）
      pid_debug_info_t empty_info = {0};
      empty_info.updated = is_cool_back_active;
      ffb_core0_update_shared(&empty_info);
    }

    // --- 共有メモリから入力を取得してHID送信 ---
    if (hidwffb_ready()) {
      custom_gamepad_report_t shared_report = {0, 0, 0, 0};
      ffb_core0_get_input_report(&shared_report);
      hidwffb_send_report(&shared_report);
    }
  }
}

// --- Core1: FFB演算およびモータ制御用 ---
FFB_Shared_State_t core1_effects[MAX_EFFECTS];

void setup1() {
  // Core1 初期化処理
  for (int i = 0; i < MAX_EFFECTS; i++) {
    core1_effects[i].active = false;
    core1_effects[i].magnitude = 0;
  }
  loop1_trigger.init();
}

void loop1() {
  // Core1 メインループ (1000Hz周期)
  if (loop1_trigger.hasExpired()) {
    custom_gamepad_report_t core1_input = {0, 0, 0, 0};

    // 物理入力読み取り (将来実装。現在は0またはループバック値)
    // hidwffb_loopback_test_sync 内で CALLBACK_TEST_ENABLE 時は steer
    // が上書きされる

    // 同期処理
    hidwffb_loopback_test_sync(&core1_input, core1_effects);

    // モータ出力演算など (将来実装)
  }
}
