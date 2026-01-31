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
// #define PID_DEBUG_ENABLE ///< PIDプロトコルのパース結果をシリアル出力する
// #define HID_INPUT_DEBUG_ENABLE ///< HID入力データをシリアル出力する

// --- 周期管理用変数 ---
uint32_t last_loop_ms = 0;           ///< Core0 メインループの最終実行時刻
const uint32_t LOOP_INTERVAL_MS = 1; ///< 1000Hz周期

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

  Serial.println("System Refactored: HID Gamepad Ready (Core0)");
  last_loop_ms = millis();
}

void loop() {
  // 1ms周期で実行 (util.h の checkInterval_m を使用)
  if (checkInterval_m(last_loop_ms, LOOP_INTERVAL_MS)) {

    if (hidwffb_ready()) {
#ifdef HID_INPUT_DEBUG_ENABLE
      static uint32_t dummy_override_until = 0;
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
            dummy_override_until = millis() + 5000;
            Serial.println("[HID_DEBUG] Dummy Data Serial Received");
          }
        }
      }

      if (millis() < dummy_override_until) {
        // ダミーデータ送信
        hidwffb_send_report(&dummy_report);
      } else {
#endif
        // --- 物理入力読み取り ---
        int16_t steer = 0; // SPIセンサ読み取り値 (将来実装)
        int32_t raw_accel = analogRead(PIN_ACCEL);
        int32_t raw_brake = analogRead(PIN_BRAKE);

        custom_gamepad_report_t report;
        report.steer = steer;
        report.accel = (int16_t)((raw_accel * 64) - 32768);
        report.brake = (int16_t)((raw_brake * 64) - 32768);
        report.buttons = 0;

        if (!digitalRead(PIN_SHIFT_UP))
          report.buttons |= (1 << 0);
        if (!digitalRead(PIN_SHIFT_DOWN))
          report.buttons |= (1 << 1);

        // --- レポート送信 ---
        hidwffb_send_report(&report);
#ifdef HID_INPUT_DEBUG_ENABLE
      }
#endif
    }

    // --- FFBデータ更新チェック ---
    if (hidwffb_get_ffb_data(current_ffb_buf)) {
      // 受信データを Core1 が参照可能な共有バッファ等へ反映
    }

    // --- PIDデバッグ出力 ---
#ifdef PID_DEBUG_ENABLE
    pid_debug_info_t pid_info;
    if (hidwffb_get_pid_debug_info(&pid_info)) {
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
      } else if (pid_info.lastReportId == 0x0D) {
        Serial.print("[PID_DEBUG] ID:0x0D, G:");
        Serial.println(pid_info.deviceGain);
      } else if (pid_info.lastReportId == 0x0A) {
        Serial.print("[PID_DEBUG] ID:0x0A, Index:");
        Serial.print(pid_info.effectBlockIndex);
        Serial.print(", Op:");
        if (pid_info.operation == 1)
          Serial.print("Start");
        else if (pid_info.operation == 2)
          Serial.print("Solo");
        else if (pid_info.operation == 3)
          Serial.print("Stop");
        else
          Serial.print(pid_info.operation);
        Serial.println();
      }
    }
#endif
  }
}

// --- Core1: FFB演算およびモータ制御用 ---
void setup1() {
  // Core1 初期化処理 (将来実装)
}

void loop1() {
  // Core1 メインループ (将来実装)
  // 角度センサの読み取りやFFB計算、PWM出力などを行う
}
