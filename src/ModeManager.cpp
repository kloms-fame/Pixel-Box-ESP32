#include "ModeManager.h"
#include <Arduino.h>

void AppState_RequestMode(AppMode newMode)
{
    // 1. 防重复切换：如果已经是目标模式，直接返回，避免浪费 CPU 和刷新屏幕
    if (AppState.currentMode == newMode)
    {
        return;
    }

    // 2. 退出清理 (Exit Cleanup)
    // 在这里集中处理因为离开某个模式导致的 UI 标志位复位问题
    if (AppState.currentMode == MODE_COUNTDOWN && AppState.isCountdownFinished)
    {
        // 如果倒计时结束了，切走时顺手复位状态，防止再次切回来时依然显示 0
        AppState.isCountdownFinished = false;
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
    }

    // 3. 记录轨迹，实现安全的“返回上一页”逻辑
    AppState.previousMode = AppState.currentMode;

    // 4. 核心：更新当前状态
    AppState.currentMode = newMode;

    // 5. 触发渲染：统一通知主循环的 Render_Tick 刷新屏幕
    AppState.needRender = true;

    // 6. 全局监控日志，让你清清楚楚知道是谁在什么时候切了屏幕
    Serial.printf("[MODE] 切屏成功: %d -> %d\n", AppState.previousMode, AppState.currentMode);
}