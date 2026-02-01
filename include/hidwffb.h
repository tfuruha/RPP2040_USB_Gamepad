/**
 * @file hidwffb.h
 * @brief HIDゲームコントローラおよびFFB（出力レポート）制御モジュール
 */

#ifndef HIDWFFB_H
#define HIDWFFB_H

#include "pico/mutex.h"
#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <stdbool.h>
#include <stdint.h>

// --- 定数定義 ---
#define HID_FFB_REPORT_SIZE 64 ///< FFB受信用レポートのバッファサイズ
#define MAX_EFFECTS 10         // 必要に応じて調整

// --- Report IDs (Host to Device) ---
#define HID_ID_SET_EFFECT 0x01
#define HID_ID_SET_ENVELOPE 0x02
#define HID_ID_SET_CONDITION 0x03
#define HID_ID_SET_PERIODIC 0x04
#define HID_ID_SET_CONSTANT_FORCE 0x05
#define HID_ID_SET_RAMP_FORCE 0x06
#define HID_ID_SET_CUSTOM_FORCE 0x07
// ...
#define HID_ID_EFFECT_OPERATION 0x0A // エフェクトのStart/Stop
#define HID_ID_DEVICE_CONTROL 0x0B   // 全停止/リセット
#define HID_ID_DEVICE_GAIN 0x0D      // 全体ゲイン

// --- Effect Types (ET) ---
#define HID_ET_CONSTANT 0x26 // Constant Force
#define HID_ET_RAMP 0x27
#define HID_ET_SQUARE 0x30
#define HID_ET_SINE 0x31
#define HID_ET_SPRING 0x40 // Spring
#define HID_ET_DAMPER 0x41 // Damper
#define HID_ET_INERTIA 0x42
#define HID_ET_FRICTION 0x43

// --- Effect Operations ---
#define HID_OP_START 0x01
#define HID_OP_SOLO 0x02
#define HID_OP_STOP 0x03

/**
 * @brief カスタム HID レポート構造体 (16bit 軸 x 3, Button x 16)
 * Core 1 -> Core 0 (物理入力/レポート用)を兼ねる
 */
typedef struct {
  int16_t steer;    ///< 操舵 (Z軸: 0x32) [-32767 to 32767]
  int16_t accel;    ///< アクセル (Rx軸: 0x33) [-32767 to 32767]
  int16_t brake;    ///< ブレーキ (Ry軸: 0x34) [-32767 to 32767]
  uint16_t buttons; ///< ボタン (16ビット分) [1:Pressed, 0:Released]
} custom_gamepad_report_t;

// --- PID (Force Feedback) レポート構造体定義 ---
// 扱うデータが密なため、__attribute__((packed)) を使用する
/**
 * @brief Set Effect Output Report (ID: 0x01)
 */
typedef struct {
  uint8_t reportId;         ///< = 0x01
  uint8_t effectBlockIndex; ///< 1..40
  uint8_t effectType;       ///< 0x26: ET Constant Force
  uint16_t duration;        ///< 0..65535 (ms)
  uint16_t triggerRepeatInterval;
  int16_t gain; ///< 0..32767
  uint8_t triggerButton;
  uint8_t enableAxis; ///< bits: 0=X, 1=Y, 2=DirectionEnable
  uint16_t direction; ///< 0..32767 (0..359.99 deg)
  uint16_t startDelay;
} __attribute__((packed)) USB_FFB_Report_SetEffect_t;

/**
 * @brief Set Effect Operation Output Report (ID: 0x0A)
 */
typedef struct {
  uint8_t reportId;         ///< = 0x0A
  uint8_t effectBlockIndex; ///< 1..40
  uint8_t operation;        ///< 1: Start, 2: StartSolo, 3: Stop
  uint8_t loopCount;        ///< 0..255
} __attribute__((packed)) USB_FFB_Report_EffectOperation_t;

/**
 * @brief Set Constant Force Output Report (ID: 0x05)
 */
typedef struct {
  uint8_t reportId;         ///< = 0x05
  uint8_t effectBlockIndex; ///< 1..40
  int16_t magnitude;        ///< -32767..32767
} __attribute__((packed)) USB_FFB_Report_SetConstantForce_t;

/**
 * @brief Device Gain Output Report (ID: 0x0D)
 */
typedef struct {
  uint8_t reportId;   ///< = 0x0D
  uint8_t deviceGain; ///< 0..255
} __attribute__((packed)) USB_FFB_Report_DeviceGain_t;

/**
 * @brief パースされたPIDデータの要約（デバッグ出力用）
 */
typedef struct {
  uint8_t lastReportId;
  bool isConstantForce;
  int16_t magnitude;
  uint8_t deviceGain;
  uint8_t operation;        ///< Added for 0x0A
  uint8_t effectBlockIndex; ///< Added for 0x0A
  bool updated;
} pid_debug_info_t;

// Core間通信用構造体
// Core 0 -> Core 1 (FFB命令)
typedef struct {
  int16_t magnitude;
  int16_t gain; ///< 0x01 で設定される Gain
  uint8_t type;
  volatile bool active;
  volatile bool isCoolBackTest; ///< 5秒間のテストモードフラグ
} FFB_Shared_State_t;

// --- 公開関数 ---

void hidwffb_begin(uint8_t poll_interval_ms = 1);
bool hidwffb_send_report(custom_gamepad_report_t *report);
bool hidwffb_is_mounted(void);
void hidwffb_wait_for_mount(void);
bool hidwffb_ready(void);
bool hidwffb_get_ffb_data(uint8_t *buffer);
void hidwffb_clear_ffb_flag(void);

void PID_ParseReport(uint8_t const *buffer, uint16_t bufsize);
bool hidwffb_get_pid_debug_info(pid_debug_info_t *info);

void ffb_shared_memory_init(); // Core間通信用構造体の初期化
void ffb_core0_update_shared(pid_debug_info_t *info);
void ffb_core0_get_input_report(custom_gamepad_report_t *dest);
void ffb_core1_update_shared(custom_gamepad_report_t *new_input,
                             FFB_Shared_State_t *local_effects_dest);
void hidwffb_loopback_test_sync(custom_gamepad_report_t *new_input,
                                FFB_Shared_State_t *local_effects_dest);
#endif // HIDWFFB_H
