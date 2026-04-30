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

// ================== 全局报警打断拦截器 ==================
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
        Serial.println("[⌨️ KEY] 任意键按下 -> 已强行打断提醒并恢复原界面");
        return true;
    }
    return false;
}

// ================== 按键事件回调 ==================

void onModeClick()
{
    if (checkAndDismissAlerts())
        return;

    int m = AppState.currentMode;
    m = (m + 1) % 6;
    AppState.currentMode = (AppMode)m;

    Display_Clear();
    Display_Show();
    WebGateway_BroadcastBasicState();
    Serial.printf("[⌨️ KEY] MODE单击 -> 切换UI模式至: %d\n", m);
}

void onPlusClick()
{
    if (checkAndDismissAlerts())
        return;

    if (AppState.currentMode == MODE_COUNTDOWN && !AppState.isCountdownRunning)
    {
        // 【核心修改】：1到60完美循环 (+1)
        uint32_t mins = AppState.countdownTotalSeconds / 60;
        mins = mins % 60 + 1;
        AppState.countdownTotalSeconds = mins * 60;
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
        WebGateway_BroadcastCdownConfig();
        Serial.printf("[⌨️ KEY] PLUS单击 -> 倒计时循环至: %d 分钟\n", mins);
    }
    else if (AppState.currentMode == MODE_ALARM)
    {
        AppState.alarmDisplayIndex = (AppState.alarmDisplayIndex + 2) % 3;
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

    if (AppState.currentMode == MODE_COUNTDOWN && !AppState.isCountdownRunning)
    {
        // 【核心修改】：1到60完美循环 (-1)
        uint32_t mins = AppState.countdownTotalSeconds / 60;
        mins = (mins - 1 - 1 + 60) % 60 + 1;
        AppState.countdownTotalSeconds = mins * 60;
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
        WebGateway_BroadcastCdownConfig();
        Serial.printf("[⌨️ KEY] MINUS单击 -> 倒计时循环至: %d 分钟\n", mins);
    }
    else if (AppState.currentMode == MODE_ALARM)
    {
        AppState.alarmDisplayIndex = (AppState.alarmDisplayIndex + 1) % 3;
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
        // 【核心修改】：长按跨越 +10 完美循环
        uint32_t mins = AppState.countdownTotalSeconds / 60;
        mins = (mins + 10 - 1) % 60 + 1;
        AppState.countdownTotalSeconds = mins * 60;
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
        WebGateway_BroadcastCdownConfig();
        Serial.printf("[⌨️ KEY] PLUS长按 -> 倒计时跃进增加至: %d 分钟\n", mins);
    }
}

void onMinusLongPress()
{
    if (checkAndDismissAlerts())
        return;

    if (AppState.currentMode == MODE_COUNTDOWN && !AppState.isCountdownRunning)
    {
        // 【核心修改】：长按跨越 -10 完美循环
        uint32_t mins = AppState.countdownTotalSeconds / 60;
        mins = (mins - 10 - 1 + 60) % 60 + 1;
        AppState.countdownTotalSeconds = mins * 60;
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
        WebGateway_BroadcastCdownConfig();
        Serial.printf("[⌨️ KEY] MINUS长按 -> 倒计时跃进减少至: %d 分钟\n", mins);
    }
}

void onOkClick()
{
    if (checkAndDismissAlerts())
        return;

    if (AppState.currentMode == MODE_SENSOR)
    {
        // 【核心修改】：短按强制发起寻呼直连
        AppState.pendingCmd = 10;
        Serial.println("[⌨️ KEY] OK单击 -> 发起直连已知骑行设备指令");
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

    if (AppState.currentMode == MODE_SENSOR)
    {
        // 【核心修改】：长按防误触断开
        if (SensorHub_GetActiveClientCount() > 0)
        {
            AppState.pendingCmd = 9;
            Serial.println("[⌨️ KEY] OK长按 -> 强行断开所有骑行设备");
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
    Serial.println("[⌨️ INIT] 物理按键防误触、循环逻辑重构完毕");
}

void ButtonManager_Loop()
{
    btnMode.tick();
    btnPlus.tick();
    btnMinus.tick();
    btnOk.tick();
}