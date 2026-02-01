/**
 * @file hidwffb.cpp
 * @brief HIDゲームコントローラおよびFFB制御モジュールの実装
 */

#include "hidwffb.h"
#include <stddef.h>
#include <string.h>

// --- HID レポート記述子 (Raw binary形式) ---
// 16bit軸 x 3 (Z, Rx, Ry), Button x 16, FFB Output Report x 64
static uint8_t const desc_hid_report[] = {
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x05,       // Usage (Gamepad)
    0xA1, 0x01,       // Collection (Application)
    0x85, 0x01,       //   Report ID (1)
    0x05, 0x01,       //   Usage Page (Generic Desktop)
    0x09, 0x32,       //   Usage (Z) - ステアリングをZ軸に（独立バー表示のため）
    0x09, 0x33,       //   Usage (Rx) - アクセル
    0x09, 0x34,       //   Usage (Ry) - ブレーキ
    0x16, 0x01, 0x80, //   Logical Minimum (-32767)
    0x26, 0xFF, 0x7F, //   Logical Maximum (32767)
    0x75, 0x10,       //   Report Size (16)
    0x95, 0x03,       //   Report Count (3)
    0x81, 0x02,       //   Input (Data, Variable, Absolute)
    0x05, 0x09,       //   Usage Page (Button)
    0x19, 0x01,       //   Usage Minimum (1)
    0x29, 0x10,       //   Usage Maximum (16)
    0x15, 0x00,       //   Logical Minimum (0)
    0x25, 0x01,       //   Logical Maximum (1)
    0x75, 0x01,       //   Report Size (1)
    0x95, 0x10,       //   Report Count (16)
    0x81, 0x02,       //   Input (Data, Variable, Absolute)

    // --- Output Reports: FFB/PID制御 (同一コレクション内) ---
    // Set Effect (ID: 1)
    0x05, 0x01, //   Usage Page (Generic Desktop)
    0x85, 0x01, //   Report ID (1)
    0x09, 0x01, //   Usage (0x01)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x0E, //   Report Count (14) - ID除くサイズ 14
    0x91, 0x02, //   Output (Data, Variable, Absolute)

    // Set Constant Force (ID: 5)
    0x85, 0x05, //   Report ID (5)
    0x09, 0x05, //   Usage (0x05)
    0x95, 0x03, //   Report Count (3) - ID除くサイズ 3
    0x91, 0x02, //   Output (Data, Variable, Absolute)

    // Device Gain (ID: 13)
    0x85, 0x0D, //   Report ID (13)
    0x09, 0x0D, //   Usage (0x0D)
    0x95, 0x01, //   Report Count (1) - ID除くサイズ 1
    0x91, 0x02, //   Output (Data, Variable, Absolute)

    // Effect Operation (ID: 10/0x0A)
    0x85, 0x0A, //   Report ID (10)
    0x09, 0x0A, //   Usage (0x0A)
    0x95, 0x03, //   Report Count (3) - ID除くサイズ 3 (Index, Op, Loop)
    0x91, 0x02, //   Output (Data, Variable, Absolute)

    // 汎用 FFB データ用 (ID: 2)
    0x06, 0x00, 0xFF, //   Usage Page (Vendor Defined 0xFF00)
    0x85, 0x02,       //   Report ID (2)
    0x09, 0x02,       //   Usage (0x02)
    0x95, 0x40,       //   Report Count (64)
    0x91, 0x02,       //   Output (Data, Variable, Absolute)

    0xC0 // End Collection
};

// USB HID インスタンス
static Adafruit_USBD_HID _usb_hid;

// FFBデータ管理用
static uint8_t _ffb_data[HID_FFB_REPORT_SIZE];
static volatile bool _ffb_updated = false;

// PIDパース状態保持用
static pid_debug_info_t _pid_debug = {0, false, 0, 0, false};
// Core 0 用のローカルデータ（パース結果の保持用）
static FFB_Shared_State_t core0_ffb_effects[MAX_EFFECTS];
static uint8_t core0_global_gain = 255;

/**
 * @brief HID受信コールバック (内部用)
 * PCから Output Report (FFB) が届いた際に呼び出される
 */
void _hid_report_callback(uint8_t report_id, hid_report_type_t report_type,
                          uint8_t const *buffer, uint16_t bufsize) {
  if (report_type == HID_REPORT_TYPE_OUTPUT) {
    // buffer には report_id が含まれない場合がある（TinyUSBの仕様による）
    // PID_ParseReport は buffer[0] が ID
    // であることを期待しているため、一時的なバッファを作成
    uint8_t temp_buf[HID_FFB_REPORT_SIZE];
    temp_buf[0] = report_id;
    uint16_t copy_size =
        (bufsize < HID_FFB_REPORT_SIZE - 1) ? bufsize : HID_FFB_REPORT_SIZE - 1;
    memcpy(&temp_buf[1], buffer, copy_size);

    // PIDパースの実行
    PID_ParseReport(temp_buf, copy_size + 1);

    // 従来の汎用バッファ更新 (Report ID 1 または 2 を想定)
    if (report_id == 1 || report_id == 2) {
      memcpy(_ffb_data, temp_buf,
             (copy_size + 1 < HID_FFB_REPORT_SIZE) ? copy_size + 1
                                                   : HID_FFB_REPORT_SIZE);
      _ffb_updated = true;
    }
  }
}

void hidwffb_begin(uint8_t poll_interval_ms) {
  _usb_hid.setPollInterval(poll_interval_ms);
  _usb_hid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
  _usb_hid.setReportCallback(NULL, _hid_report_callback);
  _usb_hid.begin();
}

bool hidwffb_is_mounted(void) { return TinyUSBDevice.mounted(); }

void hidwffb_wait_for_mount(void) {
  while (!TinyUSBDevice.mounted()) {
    delay(1);
  }
}

bool hidwffb_ready(void) {
  return TinyUSBDevice.mounted() && !TinyUSBDevice.suspended() &&
         _usb_hid.ready();
}

bool hidwffb_send_report(custom_gamepad_report_t *report) {
  if (!hidwffb_ready())
    return false;
  return _usb_hid.sendReport(1, report, sizeof(custom_gamepad_report_t));
}

bool hidwffb_get_ffb_data(uint8_t *buffer) {
  if (!_ffb_updated)
    return false;

  // Core1等からの同時アクセスに備え、コピー中はフラグを下ろすか考慮が必要だが、
  // ここでは単純なフラグ管理を行う
  memcpy(buffer, _ffb_data, HID_FFB_REPORT_SIZE);
  _ffb_updated = false;
  return true;
}

void hidwffb_clear_ffb_flag(void) { _ffb_updated = false; }

void PID_ParseReport(uint8_t const *buffer, uint16_t bufsize) {
  if (buffer == NULL || bufsize == 0)
    return;

  uint8_t reportId = buffer[0];
  _pid_debug.lastReportId = reportId;

  switch (reportId) {
  case 0x01: { // Set Effect Report
    if (bufsize >= sizeof(USB_FFB_Report_SetEffect_t)) {
      USB_FFB_Report_SetEffect_t *report = (USB_FFB_Report_SetEffect_t *)buffer;
      // ET Constant Force (0x26) のチェック
      uint8_t idx = report->effectBlockIndex - 1;
      if (idx < MAX_EFFECTS) {
        core0_ffb_effects[idx].type = report->effectType;
        core0_ffb_effects[idx].gain = report->gain; // Gainを記録
      }
      if (report->effectType == 0x26) {
        _pid_debug.isConstantForce = true;
        // SetEffect時のGainを暫定的なMagとして扱う（後のID:05で上書きされる可能性あり）
        _pid_debug.magnitude = report->gain;
      } else {
        _pid_debug.isConstantForce = false;
      }
      _pid_debug.updated = true;
    }
    break;
  }

  case 0x05: { // Set Constant Force Report
    if (bufsize >= sizeof(USB_FFB_Report_SetConstantForce_t)) {
      USB_FFB_Report_SetConstantForce_t *report =
          (USB_FFB_Report_SetConstantForce_t *)buffer;
      uint8_t idx = report->effectBlockIndex - 1;
      if (idx < MAX_EFFECTS) {
        // PCから届く -10000〜10000 の値を、内部用の -32767〜32767
        // 等にスケーリング
        core0_ffb_effects[idx].magnitude = report->magnitude;
      }
      _pid_debug.magnitude = report->magnitude;
      _pid_debug.updated = true;
    }
    break;
  }

  case 0x0A: { // Set Effect Operation Report
    if (bufsize >= sizeof(USB_FFB_Report_EffectOperation_t)) {
      USB_FFB_Report_EffectOperation_t *report =
          (USB_FFB_Report_EffectOperation_t *)buffer;
      uint8_t idx = report->effectBlockIndex - 1;
      if (idx < MAX_EFFECTS) {
        if (report->operation == HID_OP_START)
          core0_ffb_effects[idx].active = true;
        if (report->operation == HID_OP_STOP)
          core0_ffb_effects[idx].active = false;
      }
      _pid_debug.operation = report->operation;
      _pid_debug.effectBlockIndex = report->effectBlockIndex;
      _pid_debug.updated = true;
    }
    break;
  }

  case 0x0D: { // Device Gain Report
    if (bufsize >= sizeof(USB_FFB_Report_DeviceGain_t)) {
      USB_FFB_Report_DeviceGain_t *report =
          (USB_FFB_Report_DeviceGain_t *)buffer;
      core0_global_gain = report->deviceGain;
      _pid_debug.deviceGain = core0_global_gain;
      _pid_debug.updated = true;
    }
    break;
  }

  default:
    // 他のIDは現状無視
    break;
  }
}

bool hidwffb_get_pid_debug_info(pid_debug_info_t *info) {
  if (!_pid_debug.updated)
    return false;

  if (info != NULL) {
    memcpy(info, &_pid_debug, sizeof(pid_debug_info_t));
  }
  _pid_debug.updated = false;
  return true;
}
/*
双方向アクセスロジック
Core 0 と Core 1 がお互いのデータを「安全に、かつ迅速に」読み書きするための関数
*/
FFB_Shared_State_t shared_ffb_effects[MAX_EFFECTS];
volatile uint8_t shared_global_gain = 255;
custom_gamepad_report_t shared_input_report;
mutex_t ffb_shared_mutex;

// --- Core間通信用構造体の初期化 ---
void ffb_shared_memory_init() { mutex_init(&ffb_shared_mutex); }

// --- Core 0 側: パース結果を共有メモリへ反映 ---
void ffb_core0_update_shared(pid_debug_info_t *info) {
  if (mutex_enter_timeout_ms(&ffb_shared_mutex, 1)) {
    for (int i = 0; i < MAX_EFFECTS; i++) {
      shared_ffb_effects[i] = core0_ffb_effects[i];
      // Core0 側でタイマー管理しているフラグを共有メモリに反映
      if (info != NULL) {
        shared_ffb_effects[i].isCoolBackTest =
            info->updated; // 暫定：後ほど main.cpp で管理
      }
    }
    shared_global_gain = core0_global_gain;
    mutex_exit(&ffb_shared_mutex);
  }
}

// --- Core 1 側:
// 物理入力を書き込み、FFB命令を読み出す。テスト時はループバックを実施 ---
// --- Core 1 側: 物理入力(エンコーダ/ペダル)を書き込み、FFB命令を読み出す ---
void ffb_core1_update_shared(custom_gamepad_report_t *new_input,
                             FFB_Shared_State_t *local_effects_dest) {
  if (mutex_enter_timeout_ms(&ffb_shared_mutex, 1)) {
    // 1. Core 1 の結果を Core 0 へ渡す (物理入力)
    shared_input_report = *new_input;

    // 2. Core 0 の命令を Core 1 へ持ってくる (FFB命令)
    for (int i = 0; i < MAX_EFFECTS; i++) {
      local_effects_dest[i] = shared_ffb_effects[i];
    }
    mutex_exit(&ffb_shared_mutex);
  }
}

void hidwffb_loopback_test_sync(custom_gamepad_report_t *new_input,
                                FFB_Shared_State_t *local_effects_dest) {
  // 1. まず Core 0 から最新の命令を受け取る
  ffb_core1_update_shared(new_input, local_effects_dest);

#ifdef CALLBACK_TEST_ENABLE
  // 2. 受け取った命令に基づいて入力を捏造する (ループバック)
  // isCoolBackTest が true の間だけ多軸ループバックを実施
  if (local_effects_dest[0].isCoolBackTest) {
    new_input->steer = local_effects_dest[0].magnitude;
    new_input->accel = local_effects_dest[0].gain;
    new_input->brake = (int16_t)shared_global_gain; // uint8_t -> int16_t
  } else {
    // 通常時（またはテスト無効時）
    if (local_effects_dest[0].active) {
      new_input->steer = local_effects_dest[0].magnitude;
    } else {
      new_input->steer = 0;
    }
    new_input->accel = 0;
    new_input->brake = 0;
  }

  // デバッグ用
  static int16_t last_mag = 0;
  static bool last_cool = false;
  if (local_effects_dest[0].magnitude != last_mag ||
      local_effects_dest[0].isCoolBackTest != last_cool) {
    Serial.print("[CORE1_DEBUG] Mag:");
    Serial.print(local_effects_dest[0].magnitude);
    Serial.print(", CoolBack:");
    Serial.println(local_effects_dest[0].isCoolBackTest);
    last_mag = local_effects_dest[0].magnitude;
    last_cool = local_effects_dest[0].isCoolBackTest;
  }
#endif

  // 3. 捏造した(または実際の)入力を Core 0 へ戻す
  if (mutex_enter_timeout_ms(&ffb_shared_mutex, 1)) {
    shared_input_report = *new_input;
    mutex_exit(&ffb_shared_mutex);
  }
}

// --- Core 0 側: パース結果を書き込み、HID送信用の入力を読み出す ---
void ffb_core0_get_input_report(custom_gamepad_report_t *dest) {
  if (mutex_enter_timeout_ms(&ffb_shared_mutex, 1)) {
    *dest = shared_input_report;
    mutex_exit(&ffb_shared_mutex);
  }
}