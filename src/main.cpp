#include <Arduino.h>
#include <NimBLEDevice.h>
#include "Config.h"
#include "GlobalState.h"
#include "DisplayCore.h"
#include "SensorHub.h"
#include "WebGateway.h"
#include "ButtonManager.h" // 确保引入按键管理器

// ================== UI 渲染子模块 ==================
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

void renderSensor()
{
  static unsigned long lastFrame = 0;
  if (millis() - lastFrame < 100)
    return;
  lastFrame = millis();

  static unsigned long lastToggle = 0;
  static bool showHR = true;
  bool hasHR = (AppState.currentHR > 0);
  bool hasCad = (AppState.currentCadence > 0 || SensorHub_GetActiveClientCount() > 0);
  if (millis() - lastToggle > 3000)
  {
    showHR = !showHR;
    lastToggle = millis();
  }

  Display_Clear();
  if (hasHR && hasCad)
  {
    if (showHR)
      goto RENDER_HR;
    else
      goto RENDER_CAD;
  }
  else if (hasHR)
  {
  RENDER_HR:
    Display_DrawPixel(0, 2, CRGB::Red);
    Display_DrawPixel(1, 1, CRGB::Red);
    Display_DrawPixel(2, 2, CRGB::Red);
    Display_DrawPixel(1, 3, CRGB::Red);
    if (AppState.currentHR >= 100)
      Display_DrawDigit(4, 1, AppState.currentHR / 100, CRGB::Red);
    Display_DrawDigit(8, 1, (AppState.currentHR / 10) % 10, CRGB::Red);
    Display_DrawDigit(12, 1, AppState.currentHR % 10, CRGB::Red);
  }
  else
  {
  RENDER_CAD:
    CRGB c = CRGB(0, 255, 128);
    Display_DrawPixel(0, 6, c);
    Display_DrawPixel(1, 5, c);
    Display_DrawPixel(0, 4, c);
    uint16_t cad = AppState.currentCadence;
    if (cad >= 100)
    {
      Display_DrawDigit(3, 1, (cad / 100) % 10, c);
      Display_DrawDigit(7, 1, (cad / 10) % 10, c);
      Display_DrawDigit(11, 1, cad % 10, c);
    }
    else if (cad >= 10)
    {
      Display_DrawDigit(5, 1, (cad / 10) % 10, c);
      Display_DrawDigit(9, 1, cad % 10, c);
    }
    else
    {
      Display_DrawDigit(7, 1, cad % 10, c);
    }
  }

  CRGB statusColor = (SensorHub_GetActiveClientCount() > 0) ? CRGB::Green : CRGB::Red;
  Display_DrawPixel(15, 0, statusColor);

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

  // 1. 初始化顺序至关重要
  AppState.begin();
  NimBLEDevice::init(BLE_DEVICE_NAME);
  Display_Init();

  // 【修复 1】只能调用一次，否则会破坏底层 GATT 服务结构导致网页连不上
  WebGateway_Init();

  // 2. 初始化物理按键
  ButtonManager_Init();

  Serial.println("🚀 Pixel-Box OS Core Ready. [NVS + Sync + AutoRecon + Buttons]");

  // 【修复 3】不要在 setup 里阻塞扫描，防止触发看门狗重启，放入队列异步执行
  AppState.pendingCmd = 8;
}

void loop()
{
  // 【修复 2】必须在主循环调用，按键才能实时响应
  ButtonManager_Loop();

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
      SensorHub_TriggerAutoReconnect(false); // 开机自动执行 (如果不允许则跳过)
    else if (cmd == 9)
      SensorHub_DisconnectAll(); // OK按键强制一键断开所有
    else if (cmd == 10)
      SensorHub_TriggerAutoReconnect(true); // OK按键强行执行一键搜寻直连
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
        AppState.currentMode = MODE_ALARM;
        AppState.alarmDisplayIndex = i;
        Serial.printf("[⏰ ALARM FIRE] 闹钟 %d 触发！\n", i + 1);
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
      AppState.currentMode = MODE_COUNTDOWN;
      Serial.println("[⏳ CDOWN FIRE] 倒计时结束触发！");
    }
  }

  // ================= UI 分发路由 =================
  switch (AppState.currentMode)
  {
  case MODE_CLOCK:
    renderClock();
    break;
  case MODE_SENSOR:
    renderSensor();
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