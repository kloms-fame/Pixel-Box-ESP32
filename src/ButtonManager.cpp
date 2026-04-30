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

void onModeClick()
{
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
    if (AppState.currentMode == MODE_COUNTDOWN && !AppState.isCountdownRunning)
    {
        uint32_t mins = AppState.countdownTotalSeconds / 60;
        mins = (mins + 1 <= 60) ? mins + 1 : 60; // +1分钟
        AppState.countdownTotalSeconds = mins * 60;
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
        WebGateway_BroadcastCdownConfig();
        Serial.printf("[⌨️ KEY] PLUS单击 -> 倒计时增加至: %d 分钟\n", mins);
    }
    else if (AppState.currentMode == MODE_ALARM)
    {
        AppState.alarmDisplayIndex = (AppState.alarmDisplayIndex + 2) % 3; // 循环切换上一组 (+2模3等效于-1)
        Display_Clear();
        Display_Show();
        Serial.printf("[⌨️ KEY] PLUS单击 -> 检视上一组闹钟: %d\n", AppState.alarmDisplayIndex + 1);
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
        Serial.printf("[⌨️ KEY] PLUS单击 -> 亮度提升至: %d\n", b);
    }
}

void onMinusClick()
{
    if (AppState.currentMode == MODE_COUNTDOWN && !AppState.isCountdownRunning)
    {
        uint32_t mins = AppState.countdownTotalSeconds / 60;
        mins = (mins - 1 >= 1) ? mins - 1 : 1; // -1分钟
        AppState.countdownTotalSeconds = mins * 60;
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
        WebGateway_BroadcastCdownConfig();
        Serial.printf("[⌨️ KEY] MINUS单击 -> 倒计时减少至: %d 分钟\n", mins);
    }
    else if (AppState.currentMode == MODE_ALARM)
    {
        AppState.alarmDisplayIndex = (AppState.alarmDisplayIndex + 1) % 3; // 循环切换下一组
        Display_Clear();
        Display_Show();
        Serial.printf("[⌨️ KEY] MINUS单击 -> 检视下一组闹钟: %d\n", AppState.alarmDisplayIndex + 1);
    }
    else
    {
        int b = AppState.brightness - 5;
        if (b < 5)
            b = 5; // 限制最低亮度为 5
        AppState.saveBrightness(b);
        FastLED.setBrightness(b);
        FastLED.show();
        WebGateway_BroadcastBasicState();
        Serial.printf("[⌨️ KEY] MINUS单击 -> 亮度降低至: %d\n", b);
    }
}

void onPlusLongPress()
{
    if (AppState.currentMode == MODE_COUNTDOWN && !AppState.isCountdownRunning)
    {
        uint32_t mins = AppState.countdownTotalSeconds / 60;
        mins = (mins + 10 <= 60) ? mins + 10 : 60; // 跃进 +10分钟
        AppState.countdownTotalSeconds = mins * 60;
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
        WebGateway_BroadcastCdownConfig();
        Serial.printf("[⌨️ KEY] PLUS长按 -> 倒计时跃进增加至: %d 分钟\n", mins);
    }
}

void onMinusLongPress()
{
    if (AppState.currentMode == MODE_COUNTDOWN && !AppState.isCountdownRunning)
    {
        uint32_t mins = AppState.countdownTotalSeconds / 60;
        mins = (mins > 10) ? mins - 10 : 1; // 跃进 -10分钟
        AppState.countdownTotalSeconds = mins * 60;
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
        WebGateway_BroadcastCdownConfig();
        Serial.printf("[⌨️ KEY] MINUS长按 -> 倒计时跃进减少至: %d 分钟\n", mins);
    }
}

void onOkClick()
{
    // 1. 最高优先级：如果有闹钟正在响铃，单按直接强制停止
    for (int i = 0; i < 3; i++)
    {
        if (AppState.alarms[i].isRinging)
        {
            AppState.alarms[i].isRinging = false;
            Serial.printf("[⌨️ KEY] OK单击 -> 强行打断闹钟 %d 响铃\n", i + 1);
            return;
        }
    }

    // 2. 运动模式：一键接驳/断开设备
    if (AppState.currentMode == MODE_SENSOR)
    {
        if (SensorHub_GetActiveClientCount() > 0)
        {
            AppState.pendingCmd = 9; // 强行断开所有
            Serial.println("[⌨️ KEY] OK单击 -> 强行断开所有骑行设备");
        }
        else
        {
            AppState.pendingCmd = 10; // 强制发起寻呼直连
            Serial.println("[⌨️ KEY] OK单击 -> 发起直连已知骑行设备指令");
        }
    }
    // 3. 秒表模式：开始/暂停
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
    // 4. 倒计时模式：开始/暂停
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
    // 5. 闹钟模式：开启/禁用当前选中的闹钟
    else if (AppState.currentMode == MODE_ALARM)
    {
        uint8_t idx = AppState.alarmDisplayIndex;
        if (AppState.alarms[idx].isSet)
        {
            bool newState = !AppState.alarms[idx].enabled;
            AppState.saveAlarm(idx, newState, AppState.alarms[idx].hour, AppState.alarms[idx].minute);
            WebGateway_BroadcastAlarmState(idx); // 推流回网页，让网页里的 Switch 自动拨动！
            Serial.printf("[⌨️ KEY] OK单击 -> 闹钟 %d %s\n", idx + 1, newState ? "✅已启用" : "❌已禁用");
        }
        else
        {
            Serial.printf("[⌨️ KEY] 拒绝操作 -> 闹钟 %d 未配置时间！\n", idx + 1);
        }
    }
}

void onOkLongPress()
{
    if (AppState.currentMode == MODE_TIMER)
    {
        AppState.timerElapsed = 0;
        AppState.isTimerRunning = false;
        Serial.println("[⌨️ KEY] OK长按 -> 秒表重置");
    }
    else if (AppState.currentMode == MODE_COUNTDOWN)
    {
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
        AppState.isCountdownRunning = false;
        AppState.isCountdownFinished = false;
        Serial.println("[⌨️ KEY] OK长按 -> 倒计时重置");
    }
}

void ButtonManager_Init()
{
    btnMode.attachClick(onModeClick);
    btnPlus.attachClick(onPlusClick);
    btnPlus.attachLongPressStart(onPlusLongPress); // 支持+长按
    btnMinus.attachClick(onMinusClick);
    btnMinus.attachLongPressStart(onMinusLongPress); // 支持-长按
    btnOk.attachClick(onOkClick);
    btnOk.attachLongPressStart(onOkLongPress); // 支持OK长按
    Serial.println("[⌨️ INIT] 物理按键(HX-543)长短按高频矩阵挂载完成");
}

void ButtonManager_Loop()
{
    btnMode.tick();
    btnPlus.tick();
    btnMinus.tick();
    btnOk.tick();
}