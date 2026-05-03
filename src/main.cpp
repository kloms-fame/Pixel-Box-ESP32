#include <Arduino.h>
#include "EventBus.h"
#include "ModeManager.h"
#include <NimBLEDevice.h>
#include "Config.h"
#include "TimeSync.h"
#include "GlobalState.h"
#include "DisplayCore.h"
#include "SensorHub.h"
#include "WebGateway.h"
#include "ButtonManager.h"
#include "VoiceAssistant.h"

void renderClock()
{
  // ❌ 删除了 lastUpdate 限速和 AppState.needRender = true
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);

  CRGB c = CRGB(0, 180, 255);
  Display_DrawDigit(0, 1, timeinfo.tm_hour / 10, c);
  Display_DrawDigit(4, 1, timeinfo.tm_hour % 10, c);

  // 这里的秒闪烁属于“动画逻辑”，依赖时间戳，必须保留
  if (timeinfo.tm_sec % 2 == 0)
  {
    Display_DrawPixel(7, 2, c);
    Display_DrawPixel(7, 4, c);
  }
  Display_DrawDigit(9, 1, timeinfo.tm_min / 10, c);
  Display_DrawDigit(13, 1, timeinfo.tm_min % 10, c);
}

void renderSensor(bool showHRM)
{
  // ❌ 删除了 lastFrame 限速和 AppState.needRender = true
  if (showHRM)
  {
    bool isConnected = SensorHub_HasHRM();
    CRGB hrColor = CRGB::Red;
    // 动画逻辑保留
    bool isBeating = (AppState.currentHR > 0 && (millis() / 500) % 2 == 0);

    if (isBeating || !isConnected || AppState.currentHR == 0)
    {
      Display_DrawPixel(0, 2, hrColor);
      Display_DrawPixel(1, 1, hrColor);
      Display_DrawPixel(2, 2, hrColor);
      Display_DrawPixel(1, 3, hrColor);
    }

    if (isConnected)
    {
      uint8_t hr = AppState.currentHR;
      if (hr >= 100)
      {
        Display_DrawDigit(4, 1, hr / 100, hrColor);
        Display_DrawDigit(8, 1, (hr / 10) % 10, hrColor);
        Display_DrawDigit(12, 1, hr % 10, hrColor);
      }
      else if (hr >= 10)
      {
        Display_DrawDigit(6, 1, (hr / 10) % 10, hrColor);
        Display_DrawDigit(10, 1, hr % 10, hrColor);
      }
      else
        Display_DrawDigit(8, 1, hr, hrColor);
    }
    else
    {
      Display_DrawDigit(7, 1, 10, hrColor);
      Display_DrawDigit(11, 1, 10, hrColor);
    }
    CRGB statusColor = isConnected ? CRGB::Green : CRGB::Red;
    Display_DrawPixel(15, 0, statusColor);
  }
  else
  {
    bool isConnected = SensorHub_HasCSC();
    CRGB cadColor = CRGB(0, 255, 128);
    bool isPedaling = (AppState.currentCadence > 0 && (millis() / 250) % 2 == 0);

    if (isPedaling || !isConnected || AppState.currentCadence == 0)
    {
      Display_DrawPixel(0, 1, cadColor);
      Display_DrawPixel(1, 2, cadColor);
      Display_DrawPixel(2, 3, cadColor);
      Display_DrawPixel(1, 4, cadColor);
      Display_DrawPixel(0, 3, cadColor);
    }

    if (isConnected)
    {
      uint16_t cad = AppState.currentCadence;
      if (cad >= 100)
      {
        Display_DrawDigit(4, 1, (cad / 100) % 10, cadColor);
        Display_DrawDigit(8, 1, (cad / 10) % 10, cadColor);
        Display_DrawDigit(12, 1, cad % 10, cadColor);
      }
      else if (cad >= 10)
      {
        Display_DrawDigit(6, 1, (cad / 10) % 10, cadColor);
        Display_DrawDigit(10, 1, cad % 10, cadColor);
      }
      else
        Display_DrawDigit(8, 1, cad, cadColor);
    }
    else
    {
      Display_DrawDigit(7, 1, 10, cadColor);
      Display_DrawDigit(11, 1, 10, cadColor);
    }
    CRGB statusColor = isConnected ? CRGB::Green : CRGB::Red;
    Display_DrawPixel(15, 0, statusColor);
  }
}

void renderTimer()
{
  // ❌ 删除了 lastUpdate 限速
  unsigned long ms = AppState.timerElapsed;
  if (AppState.isTimerRunning)
    ms += (millis() - AppState.timerStartTime);
  uint16_t secs = ms / 1000;
  uint8_t m = (secs / 60) % 100;
  uint8_t s = secs % 60;
  CRGB c = CRGB(255, 150, 0);
  Display_DrawDigit(0, 1, m / 10, c);
  Display_DrawDigit(4, 1, m % 10, c);
  if (!AppState.isTimerRunning || (millis() / 500) % 2 == 0)
  {
    Display_DrawPixel(7, 2, c);
    Display_DrawPixel(7, 4, c);
  }
  Display_DrawDigit(9, 1, s / 10, c);
  Display_DrawDigit(13, 1, s % 10, c);
}

void renderCountdown()
{
  // ❌ 删除了 lastUpdate 限速
  uint32_t rem = AppState.countdownRemaining;
  if (AppState.isCountdownRunning)
  {
    uint32_t el = (millis() - AppState.countdownStartSysTime) / 1000;
    rem = (el < AppState.countdownRemaining) ? (AppState.countdownRemaining - el) : 0;
  }
  CRGB c = CRGB(180, 0, 255);
  if (AppState.isCountdownFinished)
  {
    if (millis() - AppState.countdownFinishSysTime <= 10000)
    {
      if ((millis() / 250) % 2 == 0)
      {
        Display_DrawDigit(0, 1, 0, c);
        Display_DrawDigit(4, 1, 0, c);
        Display_DrawPixel(7, 2, c);
        Display_DrawPixel(7, 4, c);
        Display_DrawDigit(9, 1, 0, c);
        Display_DrawDigit(13, 1, 0, c);
      }
    }
    else
    {
      Display_DrawDigit(0, 1, 0, c);
      Display_DrawDigit(4, 1, 0, c);
      Display_DrawPixel(7, 2, c);
      Display_DrawPixel(7, 4, c);
      Display_DrawDigit(9, 1, 0, c);
      Display_DrawDigit(13, 1, 0, c);
    }
  }
  else
  {
    uint8_t m = (rem / 60) % 100;
    uint8_t s = rem % 60;
    Display_DrawDigit(0, 1, m / 10, c);
    Display_DrawDigit(4, 1, m % 10, c);
    if (!AppState.isCountdownRunning || (millis() / 500) % 2 == 0)
    {
      Display_DrawPixel(7, 2, c);
      Display_DrawPixel(7, 4, c);
    }
    Display_DrawDigit(9, 1, s / 10, c);
    Display_DrawDigit(13, 1, s % 10, c);
  }
}

void renderAlarm()
{
  // ❌ 删除了 lastUpdate 限速
  AlarmData &al = AppState.alarms[AppState.alarmDisplayIndex];
  CRGB c = al.enabled ? CRGB::Green : CRGB::Red;

  if (al.isRinging)
  {
    if (millis() - al.ringStartSysTime <= 10000)
    {
      if ((millis() / 250) % 2 == 0)
      {
        Display_DrawDigit(0, 1, al.hour / 10, c);
        Display_DrawDigit(4, 1, al.hour % 10, c);
        Display_DrawPixel(7, 2, c);
        Display_DrawPixel(7, 4, c);
        Display_DrawDigit(9, 1, al.minute / 10, c);
        Display_DrawDigit(13, 1, al.minute % 10, c);
      }
    }
    else
      al.isRinging = false;
  }
  else
  {
    if (!al.isSet)
    {
      Display_DrawDigit(0, 1, 10, c);
      Display_DrawDigit(4, 1, 10, c);
      Display_DrawPixel(7, 2, c);
      Display_DrawPixel(7, 4, c);
      Display_DrawDigit(9, 1, 10, c);
      Display_DrawDigit(13, 1, 10, c);
    }
    else
    {
      Display_DrawDigit(0, 1, al.hour / 10, c);
      Display_DrawDigit(4, 1, al.hour % 10, c);
      Display_DrawPixel(7, 2, c);
      Display_DrawPixel(7, 4, c);
      Display_DrawDigit(9, 1, al.minute / 10, c);
      Display_DrawDigit(13, 1, al.minute % 10, c);
    }
    for (int i = 0; i <= AppState.alarmDisplayIndex; i++)
      Display_DrawPixel(i * 2, 7, CRGB(128, 128, 128));
  }
}

void setup()
{
  Serial.begin(115200);
  AppState.begin();

  // 🌟 修复：先初始化屏幕，点亮右上角黄色指示灯，表示正在配网校时
  Display_Init();
  Display_DrawPixel(15, 0, CRGB::Yellow);

  // 【核心执行】：在蓝牙激活前，用开机的两秒钟闪电联网校时，随后抹除 WiFi 硬件状态！
  TimeSync_Init();

  NimBLEDevice::init(BLE_DEVICE_NAME);
  WebGateway_Init();
  ButtonManager_Init();
  VoiceAssistant_Init(); // 【新增】：仅需这一行唤醒语音模块
  Serial.println("🚀 Pixel-Box OS Core Ready.");
  AppState.pendingCmd = 8;
}

// ==========================================
// 全局唯一业务逻辑入口 (中央大脑)
// 约束：所有的 AppState 修改必须在这里进行！
// ==========================================
void handleEvent(const EventMsg &e)
{
  switch (e.action)
  {
  // --- 模式切换 ---
  case ACT_MODE_SWITCH:
    AppState_RequestMode((AppMode)e.value);
    break;

  // --- 秒表业务 ---
  case ACT_TIMER_START:
    if (!AppState.isTimerRunning)
    {
      AppState.timerStartTime = millis();
      AppState.isTimerRunning = true;
    }
    break;
  case ACT_TIMER_PAUSE:
    if (AppState.isTimerRunning)
    {
      AppState.timerElapsed += millis() - AppState.timerStartTime;
      AppState.isTimerRunning = false;
    }
    break;
  case ACT_TIMER_RESET:
    AppState.timerElapsed = 0;
    AppState.isTimerRunning = false;
    break;

  // --- 倒计时业务 ---
  case ACT_CDOWN_SET:
    AppState.countdownTotalSeconds = e.value;
    AppState.countdownRemaining = AppState.countdownTotalSeconds;
    AppState.isCountdownRunning = false;
    AppState.isCountdownFinished = false;
    WebGateway_BroadcastCdownConfig(); // 同步给前端
    break;
  case ACT_CDOWN_START:
    if (!AppState.isCountdownRunning && AppState.countdownRemaining > 0)
    {
      AppState.countdownStartSysTime = millis();
      AppState.isCountdownRunning = true;
      AppState.isCountdownFinished = false;
    }
    break;
  case ACT_CDOWN_PAUSE:
    if (AppState.isCountdownRunning)
    {
      uint32_t elapsed = (millis() - AppState.countdownStartSysTime) / 1000;
      AppState.countdownRemaining = (elapsed < AppState.countdownRemaining) ? (AppState.countdownRemaining - elapsed) : 0;
      AppState.isCountdownRunning = false;
    }
    break;
  case ACT_CDOWN_RESET:
    AppState.countdownRemaining = AppState.countdownTotalSeconds;
    AppState.isCountdownRunning = false;
    AppState.isCountdownFinished = false;
    break;

  // --- 闹钟业务 ---
  case ACT_ALARM_STOP_RING:
    for (int i = 0; i < 3; i++)
    {
      if (AppState.alarms[i].isRinging)
        AppState.alarms[i].isRinging = false;
    }
    break;
  case ACT_ALARM_TOGGLE:
    if (e.value < 3 && AppState.alarms[e.value].isSet)
    {
      bool newState = !AppState.alarms[e.value].enabled;
      AppState.saveAlarm(e.value, newState, AppState.alarms[e.value].hour, AppState.alarms[e.value].minute);
      WebGateway_BroadcastAlarmState(e.value);
    }
    break;
  case ACT_ALARM_SET_INDEX:
    if (e.value < 3)
      AppState.alarmDisplayIndex = e.value;
    break;
  case ACT_ALARM_SAVE:
  {
    uint8_t idx = (e.value >> 24) & 0xFF;
    uint8_t en = (e.value >> 16) & 0xFF;
    uint8_t h = (e.value >> 8) & 0xFF;
    uint8_t m = e.value & 0xFF;
    AppState.saveAlarm(idx, en > 0, h, m);
    WebGateway_BroadcastAlarmState(idx);
    break;
  }

  // --- 硬件与系统业务 ---
  case ACT_SYS_BRIGHTNESS:
    AppState.saveBrightness(e.value);
    Display_SetBrightness(e.value);   // ✅ 建议补充：立即生效屏幕亮度
    AppState.needRender = true;       // 亮度改变，请求重绘
    WebGateway_BroadcastBasicState(); // ✅ 关键补充：同步给手机 Web 端
    break;
  case ACT_SYS_AUTOREC:
    AppState.saveAutoReconnect(e.value > 0);
    break;

  // --- 传感器底层指令 ---
  case ACT_SENSOR_CMD:
    AppState.pendingCmd = e.value; // 暂存给原有 loop 中的执行逻辑
    break;

  default:
    break;
  }

  // 每次处理完有效事件，触发屏幕刷新
  AppState.needRender = true;
}

void loop()
{
  EventMsg msg;
  while (Event_Pop(&msg))
  {
    // ✅ 1. 先打印日志（溯源用）
    Serial.print("[EVENT] type=");
    Serial.print(msg.type);
    Serial.print(" action=");
    Serial.print(msg.action);
    Serial.print(" value=");
    Serial.println(msg.value);

    // ✅ 2. 再交给中央大脑处理
    handleEvent(msg);
  }

  ButtonManager_Loop();
  VoiceAssistant_Loop();

  // ================= 异步任务队列 =================
  if (AppState.pendingCmd != 0)
  {
    int cmd = AppState.pendingCmd;
    AppState.pendingCmd = 0;

    if (cmd == 5)
    {
      SensorHub_StartScan([](uint8_t t, uint8_t at, std::string m, std::string n)
                          { WebGateway_SendScanResult(t, at, m, n); });
    }
    else if (cmd == 6)
      SensorHub_Connect(AppState.pendingAddr);
    else if (cmd == 7)
      SensorHub_Disconnect(AppState.pendingAddr);
    else if (cmd == 8)
      SensorHub_TriggerAutoReconnect(false);
    else if (cmd == 9)
      SensorHub_DisconnectAll();
    else if (cmd == 10)
      SensorHub_TriggerAutoReconnect(true);
    else if (cmd == 99)
      AppState.factoryReset();
    else if (cmd == 11)
      SensorHub_TriggerAutoReconnect(true, 1);
    else if (cmd == 12)
      SensorHub_TriggerAutoReconnect(true, 2);
    else if (cmd == 13)
      SensorHub_DisconnectType(1);
    else if (cmd == 14)
      SensorHub_DisconnectType(2);
  }

  // ================= 时间守护进程 =================
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);

  static int lastCheckedMinute = -1;
  if (timeinfo.tm_year > 100 && timeinfo.tm_min != lastCheckedMinute)
  {
    lastCheckedMinute = timeinfo.tm_min;
    for (int i = 0; i < 3; i++)
    {
      if (AppState.alarms[i].isSet && AppState.alarms[i].enabled && AppState.alarms[i].hour == timeinfo.tm_hour && AppState.alarms[i].minute == timeinfo.tm_min)
      {
        AppState.alarms[i].isRinging = true;
        AppState.alarms[i].ringStartSysTime = millis();
        if (AppState.currentMode != MODE_ALARM)
          AppState_RequestMode(MODE_ALARM); // 统一入口
      }
    }
  }

  // ================= 统一渲染管线 (修复版) =================
  static unsigned long lastRender = 0;
  if (AppState.needRender || millis() - lastRender > 33)
  {
    lastRender = millis();
    AppState.needRender = false;

    // 1. 唯一清屏点
    Display_Clear();

    // 2. 调用对应渲染函数
    switch (AppState.currentMode)
    {
    case MODE_CLOCK:
      renderClock();
      break;
    case MODE_SENSOR_HRM:
      renderSensor(true);
      break;
    case MODE_SENSOR_CSC:
      renderSensor(false);
      break;
    case MODE_TIMER:
      renderTimer();
      break;
    case MODE_COUNTDOWN:
      renderCountdown();
      break;
    case MODE_ALARM:
      renderAlarm();
      break;
    case MODE_OFF:
      break;
    }

    // 3. 唯一推屏点
    Display_Show();
  } // <--- 注意这里的括号！

  // ✅ 必须放在所有逻辑的最外层，保证每一次 loop 都能让出 CPU 10毫秒
  delay(10);
} // end of loop()