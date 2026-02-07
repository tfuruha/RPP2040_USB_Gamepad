/**
 * @file util.h
 * @brief プロジェクト内で使用する汎用処理を記載する
 * @date 2026-01-24
 *
 */

#ifndef UTIL_H
#define UTIL_H
#include <Arduino.h>
#include <cstdint>
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
/**
 * 非ブロッキング周期判定 マイクロ秒版 クラス版
 * @param IntervalTrigger_u 実行周期（マイクロ秒）
 */
class IntervalTrigger_u {
public:
  IntervalTrigger_u(uint32_t interval_us)
      : interval(interval_us), running(false) {}

  void init() { // 周期判定を開始する
    prev = micros();
    running = true;
  }

  bool hasExpired() { // 周期判定を行う
    if (!running)
      return false;
    uint32_t now = micros();
    if ((uint32_t)(now - prev) >= interval) {
      prev += interval;
      return true;
    }
    return false;
  }

private:
  uint32_t interval;
  uint32_t prev;
  bool running;
};

/**
 * 非ブロッキング周期判定 ミリ秒版 クラス版
 * @param IntervalTrigger_m 実行周期（ミリ秒）
 */
class IntervalTrigger_m {
public:
  IntervalTrigger_m(uint32_t interval_ms)
      : interval(interval_ms), running(false) {}

  void init() { // 周期判定を開始する
    prev = millis();
    running = true;
  }

  bool hasExpired() { // 周期判定を行う
    if (!running)
      return false;
    uint32_t now = millis();
    if ((uint32_t)(now - prev) >= interval) {
      prev += interval;
      return true;
    }
    return false;
  }

private:
  uint32_t interval;
  uint32_t prev;
  bool running;
};

/**
 * 非ブロッキング・ワンショット判定 マイクロ秒版 クラス版
 * @param OneShotTrigger_u 実行遅延（マイクロ秒）
 */
class OneShotTrigger_u {
public:
  OneShotTrigger_u(uint32_t delay_us) : delay(delay_us), running(false) {}

  void start() { // 期間判定を開始する
    prev = micros();
    running = true;
  }

  bool hasExpired() { // 判定を行う。一度 true を返すと次に start()
                      // が呼ばれるまで false を返す
    if (!running)
      return false;
    uint32_t now = micros();
    if ((uint32_t)(now - prev) >= delay) {
      running = false; // 一度きりの動作
      return true;
    }
    return false;
  }

  void stop() { running = false; }
  bool isRunning() const {
    return running;
  } // start() 後、期限切れになるまで true

private:
  uint32_t delay;
  uint32_t prev;
  bool running;
};

/**
 * 非ブロッキング・ワンショット判定 ミリ秒版 クラス版
 * @param OneShotTrigger_m 実行遅延（ミリ秒）
 */
class OneShotTrigger_m {
public:
  OneShotTrigger_m(uint32_t delay_ms) : delay(delay_ms), running(false) {}

  void start() { // 期間判定を開始する
    prev = millis();
    running = true;
  }

  bool hasExpired() { // 判定を行う。一度 true を返すと次に start()
                      // が呼ばれるまで false を返す
    if (!running)
      return false;
    uint32_t now = millis();
    if ((uint32_t)(now - prev) >= delay) {
      running = false; // 一度きりの動作
      return true;
    }
    return false;
  }

  void stop() { running = false; }
  bool isRunning() const {
    return running;
  } // start() 後、期限切れになるまで true

private:
  uint32_t delay;
  uint32_t prev;
  bool running;
};

#endif // UTIL_H
