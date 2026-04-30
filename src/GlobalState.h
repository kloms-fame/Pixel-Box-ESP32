#pragma once
#include "Config.h"
#include <NimBLEAddress.h>
#include <Preferences.h>

enum AppMode
{
    MODE_OFF = 0,
    MODE_CLOCK = 1,
    MODE_SENSOR = 2,
    MODE_TIMER = 3,
    MODE_COUNTDOWN = 4,
    MODE_ALARM = 5
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

// 严谨的单例模式 (Singleton) 管理全局状态与 NVS 存储
class AppStateManager
{
private:
    AppStateManager() {}
    Preferences prefs;

public:
    static AppStateManager &getInstance()
    {
        static AppStateManager instance;
        return instance;
    }

    void begin();

    // === 持久化配置 (NVS) ===
    uint8_t brightness = BRIGHTNESS_DEFAULT;
    bool autoReconnect = false;
    string savedHrmMac = "";
    string savedCscMac = "";
    AlarmData alarms[3];

    void saveBrightness(uint8_t b);
    void saveAutoReconnect(bool enabled);
    void saveAlarm(uint8_t idx, bool enabled, uint8_t h, uint8_t m);
    void saveSensorMac(uint8_t type, string mac);

    // === 易失性运行状态 (Volatile) ===
    AppMode currentMode = MODE_CLOCK;

    uint8_t currentHR = 0;
    uint16_t currentCadence = 0;
    unsigned long lastCrankSysTime = 0;

    bool isTimerRunning = false;
    unsigned long timerStartTime = 0;
    unsigned long timerElapsed = 0;

    bool isCountdownRunning = false;
    bool isCountdownFinished = false;
    uint32_t countdownTotalSeconds = 0;
    uint32_t countdownRemaining = 0;
    unsigned long countdownStartSysTime = 0;
    unsigned long countdownFinishSysTime = 0;

    uint8_t alarmDisplayIndex = 0;

    volatile int pendingCmd = 0;
    NimBLEAddress pendingAddr;
};

#define AppState AppStateManager::getInstance()