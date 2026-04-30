#include "ButtonManager.h"
#include "Config.h"
#include "GlobalState.h"
#include "DisplayCore.h"
#include "WebGateway.h"
#include <OneButton.h>

// 初始化 4 个按键 (引脚, 低电平触发, 开启内部上拉)
OneButton btnMode(BTN_MODE_PIN, true, true);
OneButton btnPlus(BTN_PLUS_PIN, true, true);
OneButton btnMinus(BTN_MINUS_PIN, true, true);
OneButton btnOk(BTN_OK_PIN, true, true);

// ================== 按键事件回调 ==================

void onModeClick()
{
    int m = AppState.currentMode;
    m = (m + 1) % 6; // 6个模式循环切换
    AppState.currentMode = (AppMode)m;

    Display_Clear();
    Display_Show();
    WebGateway_BroadcastBasicState(); // 通知 Web 端状态改变
    Serial.printf("[⌨️ KEY] MODE单击 -> 切换UI模式至: %d\n", m);
}

void onPlusClick()
{
    if (AppState.currentMode == MODE_COUNTDOWN && !AppState.isCountdownRunning)
    {
        uint32_t mins = AppState.countdownTotalSeconds / 60;
        if (mins < 60)
            mins++;
        AppState.countdownTotalSeconds = mins * 60;
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
        WebGateway_BroadcastCdownConfig(); // 通知 Web 端滑块右移
        Serial.printf("[⌨️ KEY] PLUS单击 -> 倒计时增加至: %d 分钟\n", mins);
    }
    else
    {
        int b = AppState.brightness + 10;
        if (b > 255)
            b = 255;
        AppState.saveBrightness(b);
        FastLED.setBrightness(b);
        FastLED.show();
        WebGateway_BroadcastBasicState(); // 通知 Web 端亮度滑块右移
        Serial.printf("[⌨️ KEY] PLUS单击 -> 亮度提升至: %d\n", b);
    }
}

void onMinusClick()
{
    if (AppState.currentMode == MODE_COUNTDOWN && !AppState.isCountdownRunning)
    {
        uint32_t mins = AppState.countdownTotalSeconds / 60;
        if (mins > 1)
            mins--;
        AppState.countdownTotalSeconds = mins * 60;
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
        WebGateway_BroadcastCdownConfig(); // 通知 Web 端滑块左移
        Serial.printf("[⌨️ KEY] MINUS单击 -> 倒计时减少至: %d 分钟\n", mins);
    }
    else
    {
        int b = AppState.brightness - 10;
        if (b < 0)
            b = 0;
        AppState.saveBrightness(b);
        FastLED.setBrightness(b);
        FastLED.show();
        WebGateway_BroadcastBasicState(); // 通知 Web 端亮度滑块左移
        Serial.printf("[⌨️ KEY] MINUS单击 -> 亮度降低至: %d\n", b);
    }
}

void onOkClick()
{
    // 1. 最高优先级：如果闹钟在响，单按直接关闭响铃
    for (int i = 0; i < 3; i++)
    {
        if (AppState.alarms[i].isRinging)
        {
            AppState.alarms[i].isRinging = false;
            Serial.printf("[⌨️ KEY] OK单击 -> 强行打断闹钟 %d 响铃\n", i + 1);
            return;
        }
    }

    // 2. 秒表模式：开始/暂停
    if (AppState.currentMode == MODE_TIMER)
    {
        if (!AppState.isTimerRunning)
        {
            AppState.timerStartTime = millis();
            AppState.isTimerRunning = true;
            Serial.println("[⌨️ KEY] OK单击 -> 秒表开始");
        }
        else
        {
            AppState.timerElapsed += millis() - AppState.timerStartTime;
            AppState.isTimerRunning = false;
            Serial.println("[⌨️ KEY] OK单击 -> 秒表暂停");
        }
    }
    // 3. 倒计时模式：开始/暂停
    else if (AppState.currentMode == MODE_COUNTDOWN)
    {
        if (!AppState.isCountdownRunning && AppState.countdownRemaining > 0)
        {
            AppState.countdownStartSysTime = millis();
            AppState.isCountdownRunning = true;
            AppState.isCountdownFinished = false;
            Serial.println("[⌨️ KEY] OK单击 -> 倒计时开始");
        }
        else if (AppState.isCountdownRunning)
        {
            uint32_t elapsed = (millis() - AppState.countdownStartSysTime) / 1000;
            AppState.countdownRemaining = (elapsed < AppState.countdownRemaining) ? (AppState.countdownRemaining - elapsed) : 0;
            AppState.isCountdownRunning = false;
            Serial.println("[⌨️ KEY] OK单击 -> 倒计时暂停");
        }
    }
}

void onOkLongPress()
{
    if (AppState.currentMode == MODE_TIMER)
    {
        AppState.timerElapsed = 0;
        AppState.isTimerRunning = false;
        Serial.println("[⌨️ KEY] OK长按 -> 秒表重置归零");
    }
    else if (AppState.currentMode == MODE_COUNTDOWN)
    {
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
        AppState.isCountdownRunning = false;
        AppState.isCountdownFinished = false;
        Serial.println("[⌨️ KEY] OK长按 -> 倒计时重置复位");
    }
}

// ================== 生命周期绑定 ==================

void ButtonManager_Init()
{
    btnMode.attachClick(onModeClick);
    btnPlus.attachClick(onPlusClick);
    btnMinus.attachClick(onMinusClick);
    btnOk.attachClick(onOkClick);
    btnOk.attachLongPressStart(onOkLongPress);
    Serial.println("[⌨️ INIT] 物理按键集群(HX-543)已挂载");
}

void ButtonManager_Loop()
{
    btnMode.tick();
    btnPlus.tick();
    btnMinus.tick();
    btnOk.tick();
}