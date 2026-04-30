#include "ButtonManager.h"
#include "Config.h"
#include "GlobalState.h"
#include "DisplayCore.h"
#include "WebGateway.h"
#include "SensorHub.h"
#include <OneButton.h>

OneButton btnMode(BTN_MODE_PIN, true, true);
OneButton btnPlus(BTN_PLUS_PIN, true, true);
OneButton btnMinus(BTN_MINUS_PIN, true, true);
OneButton btnOk(BTN_OK_PIN, true, true);

bool checkAndDismissAlerts()
{
    bool alerted = false;
    for (int i = 0; i < 3; i++)
    {
        if (AppState.alarms[i].isRinging)
        {
            AppState.alarms[i].isRinging = false;
            alerted = true;
        }
    }
    if (AppState.isCountdownFinished)
    {
        AppState.isCountdownFinished = false;
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
        alerted = true;
    }
    if (alerted)
    {
        AppState.currentMode = AppState.previousMode;
        Display_Clear();
        Display_Show();
        WebGateway_BroadcastBasicState();
        return true;
    }
    return false;
}

void onModeClick()
{
    if (checkAndDismissAlerts())
        return;

    int m = AppState.currentMode;

    // 【智能跨越逻辑】：把心率和踏频当作一个模块跳跃
    if (m == MODE_CLOCK)
    {
        // 如果只有踏频连着，就进踏频；否则进心率
        m = (SensorHub_HasCSC() && !SensorHub_HasHRM()) ? MODE_SENSOR_CSC : MODE_SENSOR_HRM;
    }
    else if (m == MODE_SENSOR_HRM || m == MODE_SENSOR_CSC)
    {
        m = MODE_TIMER;
    }
    else if (m == MODE_TIMER)
        m = MODE_COUNTDOWN;
    else if (m == MODE_COUNTDOWN)
        m = MODE_ALARM;
    else if (m == MODE_ALARM)
        m = MODE_OFF;
    else if (m == MODE_OFF)
        m = MODE_CLOCK;

    AppState.currentMode = (AppMode)m;
    Display_Clear();
    Display_Show();
    WebGateway_BroadcastBasicState();
}

void onPlusClick()
{
    if (checkAndDismissAlerts())
        return;

    if (AppState.currentMode == MODE_SENSOR_HRM || AppState.currentMode == MODE_SENSOR_CSC)
    {
        AppState.currentMode = (AppState.currentMode == MODE_SENSOR_HRM) ? MODE_SENSOR_CSC : MODE_SENSOR_HRM;
        Display_Clear();
        Display_Show();
        WebGateway_BroadcastBasicState();
    }
    else if (AppState.currentMode == MODE_COUNTDOWN && !AppState.isCountdownRunning)
    {
        uint32_t mins = AppState.countdownTotalSeconds / 60;
        mins = mins % 60 + 1;
        AppState.countdownTotalSeconds = mins * 60;
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
        WebGateway_BroadcastCdownConfig();
    }
    else if (AppState.currentMode == MODE_ALARM)
    {
        AppState.alarmDisplayIndex = (AppState.alarmDisplayIndex + 1) % 3;
        Display_Clear();
        Display_Show();
    }
    else
    {
        int b = AppState.brightness + 5;
        if (b > 255)
            b = 255;
        AppState.saveBrightness(b);
        FastLED.setBrightness(b);
        FastLED.show();
        WebGateway_BroadcastBasicState();
    }
}

void onMinusClick()
{
    if (checkAndDismissAlerts())
        return;

    if (AppState.currentMode == MODE_SENSOR_HRM || AppState.currentMode == MODE_SENSOR_CSC)
    {
        AppState.currentMode = (AppState.currentMode == MODE_SENSOR_HRM) ? MODE_SENSOR_CSC : MODE_SENSOR_HRM;
        Display_Clear();
        Display_Show();
        WebGateway_BroadcastBasicState();
    }
    else if (AppState.currentMode == MODE_COUNTDOWN && !AppState.isCountdownRunning)
    {
        uint32_t mins = AppState.countdownTotalSeconds / 60;
        mins = (mins - 1 - 1 + 60) % 60 + 1;
        AppState.countdownTotalSeconds = mins * 60;
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
        WebGateway_BroadcastCdownConfig();
    }
    else if (AppState.currentMode == MODE_ALARM)
    {
        AppState.alarmDisplayIndex = (AppState.alarmDisplayIndex + 2) % 3;
        Display_Clear();
        Display_Show();
    }
    else
    {
        int b = AppState.brightness - 5;
        if (b < 5)
            b = 5;
        AppState.saveBrightness(b);
        FastLED.setBrightness(b);
        FastLED.show();
        WebGateway_BroadcastBasicState();
    }
}

void onPlusLongPress()
{
    if (checkAndDismissAlerts())
        return;
    if (AppState.currentMode == MODE_COUNTDOWN && !AppState.isCountdownRunning)
    {
        uint32_t mins = AppState.countdownTotalSeconds / 60;
        mins = (mins + 10 - 1) % 60 + 1;
        AppState.countdownTotalSeconds = mins * 60;
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
        WebGateway_BroadcastCdownConfig();
    }
}

void onMinusLongPress()
{
    if (checkAndDismissAlerts())
        return;
    if (AppState.currentMode == MODE_COUNTDOWN && !AppState.isCountdownRunning)
    {
        uint32_t mins = AppState.countdownTotalSeconds / 60;
        mins = (mins - 10 - 1 + 60) % 60 + 1;
        AppState.countdownTotalSeconds = mins * 60;
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
        WebGateway_BroadcastCdownConfig();
    }
}

void onOkClick()
{
    if (checkAndDismissAlerts())
        return;

    if (AppState.currentMode == MODE_SENSOR_HRM || AppState.currentMode == MODE_SENSOR_CSC)
    {
        AppState.pendingCmd = 10;
    }
    else if (AppState.currentMode == MODE_TIMER)
    {
        if (!AppState.isTimerRunning)
        {
            AppState.timerStartTime = millis();
            AppState.isTimerRunning = true;
        }
        else
        {
            AppState.timerElapsed += millis() - AppState.timerStartTime;
            AppState.isTimerRunning = false;
        }
    }
    else if (AppState.currentMode == MODE_COUNTDOWN)
    {
        if (!AppState.isCountdownRunning && AppState.countdownRemaining > 0)
        {
            AppState.countdownStartSysTime = millis();
            AppState.isCountdownRunning = true;
            AppState.isCountdownFinished = false;
        }
        else if (AppState.isCountdownRunning)
        {
            uint32_t elapsed = (millis() - AppState.countdownStartSysTime) / 1000;
            AppState.countdownRemaining = (elapsed < AppState.countdownRemaining) ? (AppState.countdownRemaining - elapsed) : 0;
            AppState.isCountdownRunning = false;
        }
    }
    else if (AppState.currentMode == MODE_ALARM)
    {
        uint8_t idx = AppState.alarmDisplayIndex;
        if (AppState.alarms[idx].isSet)
        {
            bool newState = !AppState.alarms[idx].enabled;
            AppState.saveAlarm(idx, newState, AppState.alarms[idx].hour, AppState.alarms[idx].minute);
            WebGateway_BroadcastAlarmState(idx);
        }
    }
}

void onOkLongPress()
{
    if (checkAndDismissAlerts())
        return;

    if (AppState.currentMode == MODE_SENSOR_HRM || AppState.currentMode == MODE_SENSOR_CSC)
    {
        if (SensorHub_GetActiveClientCount() > 0)
        {
            AppState.pendingCmd = 9;
        }
    }
    else if (AppState.currentMode == MODE_TIMER)
    {
        AppState.timerElapsed = 0;
        AppState.isTimerRunning = false;
    }
    else if (AppState.currentMode == MODE_COUNTDOWN)
    {
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
        AppState.isCountdownRunning = false;
        AppState.isCountdownFinished = false;
    }
}

void ButtonManager_Init()
{
    btnMode.attachClick(onModeClick);
    btnPlus.attachClick(onPlusClick);
    btnPlus.attachLongPressStart(onPlusLongPress);
    btnMinus.attachClick(onMinusClick);
    btnMinus.attachLongPressStart(onMinusLongPress);
    btnOk.attachClick(onOkClick);
    btnOk.attachLongPressStart(onOkLongPress);
}

void ButtonManager_Loop()
{
    btnMode.tick();
    btnPlus.tick();
    btnMinus.tick();
    btnOk.tick();
}