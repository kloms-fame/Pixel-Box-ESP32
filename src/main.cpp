#include <Arduino.h>
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
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= 500)
  {
    lastUpdate = millis();
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    Display_Clear();
    CRGB c = CRGB(0, 180, 255);
    Display_DrawDigit(0, 1, timeinfo.tm_hour / 10, c);
    Display_DrawDigit(4, 1, timeinfo.tm_hour % 10, c);
    if (timeinfo.tm_sec % 2 == 0)
    {
      Display_DrawPixel(7, 2, c);
      Display_DrawPixel(7, 4, c);
    }
    Display_DrawDigit(9, 1, timeinfo.tm_min / 10, c);
    Display_DrawDigit(13, 1, timeinfo.tm_min % 10, c);
    Display_Show();
  }
}

// 【修复核心 3】完美剥离的运动面板显示逻辑
void renderSensor(bool showHRM)
{
  static unsigned long lastFrame = 0;
  if (millis() - lastFrame < 100)
    return;
  lastFrame = millis();

  Display_Clear();

  if (showHRM)
  {
    bool isConnected = SensorHub_HasHRM();
    CRGB hrColor = CRGB::Red;
    bool isBeating = (AppState.currentHR > 0 && (millis() / 500) % 2 == 0);

    // 渲染心形图标
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
      {
        Display_DrawDigit(8, 1, hr, hrColor);
      }
    }
    else
    {
      Display_DrawDigit(7, 1, 10, hrColor);
      Display_DrawDigit(11, 1, 10, hrColor);
    }

    // 右上角指示灯：只看心率是否连着
    CRGB statusColor = isConnected ? CRGB::Green : CRGB::Red;
    Display_DrawPixel(15, 0, statusColor);
  }
  else
  {
    bool isConnected = SensorHub_HasCSC();
    CRGB cadColor = CRGB(0, 255, 128);
    bool isPedaling = (AppState.currentCadence > 0 && (millis() / 250) % 2 == 0);

    // 渲染曲柄图标
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
      {
        Display_DrawDigit(8, 1, cad, cadColor);
      }
    }
    else
    {
      Display_DrawDigit(7, 1, 10, cadColor);
      Display_DrawDigit(11, 1, 10, cadColor);
    }

    // 右上角指示灯：只看踏频是否连着
    CRGB statusColor = isConnected ? CRGB::Green : CRGB::Red;
    Display_DrawPixel(15, 0, statusColor);
  }

  Display_Show();
}

void renderTimer()
{
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= 100)
  {
    lastUpdate = millis();
    unsigned long ms = AppState.timerElapsed;
    if (AppState.isTimerRunning)
      ms += (millis() - AppState.timerStartTime);
    uint16_t secs = ms / 1000;
    uint8_t m = (secs / 60) % 100;
    uint8_t s = secs % 60;
    Display_Clear();
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
    Display_Show();
  }
}

void renderCountdown()
{
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= 100)
  {
    lastUpdate = millis();
    uint32_t rem = AppState.countdownRemaining;
    if (AppState.isCountdownRunning)
    {
      uint32_t el = (millis() - AppState.countdownStartSysTime) / 1000;
      rem = (el < AppState.countdownRemaining) ? (AppState.countdownRemaining - el) : 0;
    }
    Display_Clear();
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
    Display_Show();
  }
}

void renderAlarm()
{
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= 100)
  {
    lastUpdate = millis();
    Display_Clear();
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
      {
        al.isRinging = false;
      }
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
    Display_Show();
  }
}

void setup()
{
  Serial.begin(115200);
  AppState.begin();

  // 🌟 修复：先初始化屏幕，点亮右上角黄色指示灯，表示正在配网校时
  Display_Init();
  Display_DrawPixel(15, 0, CRGB::Yellow);
  Display_Show();

  // 【核心执行】：在蓝牙激活前，用开机的两秒钟闪电联网校时，随后抹除 WiFi 硬件状态！
  TimeSync_Init();

  NimBLEDevice::init(BLE_DEVICE_NAME);
  WebGateway_Init();
  ButtonManager_Init();
  VoiceAssistant_Init(); // 【新增】：仅需这一行唤醒语音模块
  Serial.println("🚀 Pixel-Box OS Core Ready.");
  AppState.pendingCmd = 8;
}

void loop()
{
  ButtonManager_Loop();

  // 【终极修复】：必须加上这一行！让单片机每一毫秒都在静默监听语音模块的串口数据！
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
      AppState.factoryReset(); // 【新增】在安全的上下文环境中执行重置
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
          AppState.previousMode = AppState.currentMode;
        AppState.currentMode = MODE_ALARM;
        AppState.alarmDisplayIndex = i;
      }
    }
  }

  if (millis() - AppState.lastCrankSysTime > 3000 && AppState.currentCadence != 0)
    AppState.currentCadence = 0;

  if (AppState.isCountdownRunning)
  {
    uint32_t elapsed = (millis() - AppState.countdownStartSysTime) / 1000;
    if (elapsed >= AppState.countdownRemaining)
    {
      AppState.countdownRemaining = 0;
      AppState.isCountdownRunning = false;
      AppState.isCountdownFinished = true;
      AppState.countdownFinishSysTime = millis();
      if (AppState.currentMode != MODE_COUNTDOWN)
        AppState.previousMode = AppState.currentMode;
      AppState.currentMode = MODE_COUNTDOWN;
    }
  }

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

  delay(10);
}