#include "ButtonManager.h"
#include "ModeManager.h"
#include "EventBus.h"
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
        AppState_RequestMode(AppState.previousMode);
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

    EventMsg e;
    e.type = EVT_BTN;
    e.action = ACT_MODE_SWITCH;
    e.value = m;
    Event_Push(e);

    // ❌ Phase B：注释掉重复的直接调用
    /*
    AppState_RequestMode((AppMode)m);
    WebGateway_BroadcastBasicState();
    */
}

void onPlusClick()
{
    if (checkAndDismissAlerts())
        return;

    if (AppState.currentMode == MODE_SENSOR_HRM || AppState.currentMode == MODE_SENSOR_CSC)
    {
        AppState_RequestMode((AppState.currentMode == MODE_SENSOR_HRM) ? MODE_SENSOR_CSC : MODE_SENSOR_HRM); // ✅ 统一入口
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
        AppState.needRender = true; // ✅ 仅触发渲染，由 main 循环统一刷新
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

    if (AppState.currentMode == MODE_SENSOR_HRM)
    {
        AppState.pendingCmd = 11; // 仅寻呼心率
    }
    else if (AppState.currentMode == MODE_SENSOR_CSC)
    {
        AppState.pendingCmd = 12; // 仅寻呼踏频
    }
    else if (AppState.currentMode == MODE_TIMER)
    {
        if (!AppState.isTimerRunning)
        {
            EventMsg e = {EVT_BTN, ACT_TIMER_START, 0};
            Event_Push(e);
        }
        else
        {
            EventMsg e = {EVT_BTN, ACT_TIMER_PAUSE, 0};
            Event_Push(e);
        }
    }
    else if (AppState.currentMode == MODE_COUNTDOWN)
    {
        // ✅ Phase B：改为事件
        if (!AppState.isCountdownRunning && AppState.countdownRemaining > 0)
        {
            EventMsg e = {EVT_BTN, ACT_CDOWN_START, 0};
            Event_Push(e);
        }
        else if (AppState.isCountdownRunning)
        {
            EventMsg e = {EVT_BTN, ACT_CDOWN_PAUSE, 0};
            Event_Push(e);
        }
    }
    else if (AppState.currentMode == MODE_ALARM)
    {
        // ✅ Phase B：改为事件 (使用 TOGGLE)
        EventMsg e = {EVT_BTN, ACT_ALARM_TOGGLE, AppState.alarmDisplayIndex};
        Event_Push(e);
    }
}

void onOkLongPress()
{
    if (checkAndDismissAlerts())
        return;

    if (AppState.currentMode == MODE_SENSOR_HRM)
    {
        AppState.pendingCmd = 13; // 仅断开心率
    }
    else if (AppState.currentMode == MODE_SENSOR_CSC)
    {
        AppState.pendingCmd = 14; // 仅断开踏频
    }
    else if (AppState.currentMode == MODE_TIMER)
    {
        // ✅ Phase B：改为事件
        EventMsg e = {EVT_BTN, ACT_TIMER_RESET, 0};
        Event_Push(e);
    }
    else if (AppState.currentMode == MODE_COUNTDOWN)
    {
        // ✅ Phase B：改为事件
        EventMsg e = {EVT_BTN, ACT_CDOWN_RESET, 0};
        Event_Push(e);
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