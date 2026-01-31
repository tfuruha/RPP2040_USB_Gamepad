/**
 * @file util.h
 * @brief プロジェクト内で使用する汎用処理を記載する
 * @date 2026-01-24
 *
 */

#ifndef UTIL_H
#define UTIL_H
#include <Arduino.h>
/**
 * 非ブロッキング周期判定 マイクロ秒版
 * @param last_us タスクごとの最終実行時刻（参照渡し）
 * @param interval_us 実行周期（マイクロ秒）
 */
inline bool checkInterval_u(uint32_t &last_us, uint32_t interval_us) {
  uint32_t now = micros();
  if (now - last_us >= interval_us) {
    last_us += interval_us; // 基準時刻をインターバル分進める
    return true;
  }
  return false;
}

/**
 * 非ブロッキング周期判定 ミリ秒版
 * @param last_ms タスクごとの最終実行時刻（参照渡し）
 * @param interval_ms 実行周期（ミリ秒）
 */
inline bool checkInterval_m(uint32_t &last_ms, uint32_t interval_ms) {
  uint32_t now = millis();
  if (now - last_ms >= interval_ms) {
    last_ms += interval_ms; // 基準時刻をインターバル分進める
    return true;
  }
  return false;
}

#endif // UTIL_H
