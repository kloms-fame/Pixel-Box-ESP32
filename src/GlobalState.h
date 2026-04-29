#pragma once
#include "Config.h"
#include <NimBLEAddress.h>

// UI 模式枚举
enum AppMode
{
    MODE_OFF,
    MODE_CLOCK,
    MODE_SENSOR,
    MODE_TIMER,
    MODE_COUNTDOWN,
    MODE_ALARM
};

struct AlarmData
{
    bool isSet = false;
    bool enabled = false;
    uint8_t hour = 0;
    uint8_t minute = 0;
    bool isRinging = false;
    unsigned long ringStartSysTime = 0;
};

// 全局状态缓存
struct SharedState
{
    AppMode currentMode = MODE_CLOCK;

    // 传感器数据
    uint8_t currentHR = 0;
    uint16_t currentCadence = 0;
    unsigned long lastCrankSysTime = 0;

    // 正向秒表
    bool isTimerRunning = false;
    unsigned long timerStartTime = 0;
    unsigned long timerElapsed = 0;

    // 倒计时
    bool isCountdownRunning = false;
    bool isCountdownFinished = false;
    uint32_t countdownTotalSeconds = 0;
    uint32_t countdownRemaining = 0;
    unsigned long countdownStartSysTime = 0;
    unsigned long countdownFinishSysTime = 0;

    // 闹钟
    AlarmData alarms[3];
    uint8_t alarmDisplayIndex = 0;

    // 异步任务解耦队列
    volatile int pendingCmd = 0;
    NimBLEAddress pendingAddr;
};

// 暴露给所有模块的全局实例
extern SharedState AppState;