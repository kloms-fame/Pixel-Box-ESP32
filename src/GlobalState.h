#pragma once
#include "Config.h"
#include <NimBLEAddress.h>
#include <Preferences.h>
#include <string>

using namespace std;

enum AppMode
{
    MODE_OFF = 0,
    MODE_CLOCK = 1,
    MODE_SENSOR_HRM = 2,
    MODE_SENSOR_CSC = 3,
    MODE_TIMER = 4,
    MODE_COUNTDOWN = 5,
    MODE_ALARM = 6
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

    uint8_t brightness = BRIGHTNESS_DEFAULT;
    bool autoReconnect = false;
    string savedHrmMac = "";
    string savedCscMac = "";

    // 【新增】WiFi 凭据缓存
    string wifiSSID = "";
    string wifiPass = "";

    AlarmData alarms[3];

    void saveBrightness(uint8_t b);
    void saveAutoReconnect(bool enabled);
    void saveAlarm(uint8_t idx, bool enabled, uint8_t h, uint8_t m);
    void saveSensorMac(uint8_t type, string mac);
    void clearSensorMac(uint8_t type);

    // 【新增】保存配置函数
    void saveWiFi(string ssid, string pass);

    void factoryReset();

    AppMode currentMode = MODE_CLOCK;
    AppMode previousMode = MODE_CLOCK;
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