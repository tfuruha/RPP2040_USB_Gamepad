/**
 * @file hidwffb.h
 * @brief HIDゲームコントローラおよびFFB（出力レポート）制御モジュール
 */

#ifndef HIDWFFB_H
#define HIDWFFB_H

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <stdint.h>

// --- 定数定義 ---
#define HID_FFB_REPORT_SIZE 64 ///< FFB受信用レポートのバッファサイズ

/**
 * @brief カスタム HID レポート構造体 (16bit 軸 x 3, Button x 16)
 */
typedef struct {
  int16_t steer;    ///< 操舵 (Z軸: 0x32) [-32767 to 32767]
  int16_t accel;    ///< アクセル (Rx軸: 0x33) [-32767 to 32767]
  int16_t brake;    ///< ブレーキ (Ry軸: 0x34) [-32767 to 32767]
  uint16_t buttons; ///< ボタン (16ビット分) [1:Pressed, 0:Released]
} TU_ATTR_PACKED custom_gamepad_report_t;

// --- PID (Force Feedback) レポート構造体定義 ---

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
} TU_ATTR_PACKED USB_FFB_Report_SetEffect_t;

/**
 * @brief Set Constant Force Output Report (ID: 0x05)
 */
typedef struct {
  uint8_t reportId;         ///< = 0x05
  uint8_t effectBlockIndex; ///< 1..40
  int16_t magnitude;        ///< -32767..32767
} TU_ATTR_PACKED USB_FFB_Report_SetConstantForce_t;

/**
 * @brief Device Gain Output Report (ID: 0x0D)
 */
typedef struct {
  uint8_t reportId;   ///< = 0x0D
  uint8_t deviceGain; ///< 0..255
} TU_ATTR_PACKED USB_FFB_Report_DeviceGain_t;

/**
 * @brief パースされたPIDデータの要約（デバッグ出力用）
 */
typedef struct {
  uint8_t lastReportId;
  bool isConstantForce;
  int16_t magnitude;
  uint8_t deviceGain;
  bool updated;
} pid_debug_info_t;

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

#endif // HIDWFFB_H
