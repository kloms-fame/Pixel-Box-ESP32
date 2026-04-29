#include <Arduino.h>
#include <FastLED.h>
#include <NimBLEDevice.h>
#include <sys/time.h>
#include "Config.h" // 严格保持单一真值源

// ==================== 全局状态与句柄 ====================
CRGB leds[NUM_LEDS];
NimBLEServer *pServer = nullptr;
NimBLECharacteristic *pRxCharacteristic = nullptr;
NimBLECharacteristic *pTxCharacteristic = nullptr;

// 界面状态机
bool isClockMode = false;
bool isSensorMode = false;
bool isTimerMode = false;
bool isCountdownMode = false;

// 传感器数据缓存
uint8_t currentHR = 0;
uint16_t currentCadence = 0;
uint16_t lastCrankRevs = 0;
uint16_t lastCrankTime = 0;
unsigned long lastCrankSysTime = 0;

// 秒表 (正向计时) 状态
bool isTimerRunning = false;
unsigned long timerStartTime = 0;
unsigned long timerElapsed = 0;

// 倒计时状态
bool isCountdownRunning = false;
bool isCountdownFinished = false;
uint32_t countdownTotalSeconds = 0;
uint32_t countdownRemaining = 0; // 剩余秒数
unsigned long countdownStartSysTime = 0;
unsigned long countdownFinishSysTime = 0;

// 已连接的外设客户端列表
vector<NimBLEClient *> activeClients;

// --- 异步任务解耦变量 ---
volatile int pendingCmd = 0;
NimBLEAddress pendingAddr;

// 3x5 冷峻风像素字体
static const uint8_t digits[10][5] = {
    {0x07, 0x05, 0x05, 0x05, 0x07}, {0x01, 0x01, 0x01, 0x01, 0x01}, {0x07, 0x01, 0x07, 0x04, 0x07}, {0x07, 0x01, 0x07, 0x01, 0x07}, {0x05, 0x05, 0x07, 0x01, 0x01}, {0x07, 0x04, 0x07, 0x01, 0x07}, {0x07, 0x04, 0x07, 0x05, 0x07}, {0x07, 0x01, 0x01, 0x01, 0x01}, {0x07, 0x05, 0x07, 0x05, 0x07}, {0x07, 0x05, 0x07, 0x01, 0x07}};

uint16_t getIndex(uint8_t x, uint8_t y)
{
  if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT)
    return 0;
  return (x < 8) ? (y * 8 + x) : (64 + y * 8 + (x - 8));
}

void drawPixel(int x, int y, CRGB color)
{
  if (x >= 0 && x < MATRIX_WIDTH && y >= 0 && y < MATRIX_HEIGHT)
    leds[getIndex(x, y)] = color;
}

void drawDigit(int x, int y, int digit, CRGB color)
{
  if (digit < 0 || digit > 9)
    return;
  for (int r = 0; r < 5; r++)
    for (int c = 0; c < 3; c++)
      if (digits[digit][r] & (1 << (2 - c)))
        drawPixel(x + c, y + r, color);
}

// ==================== 传感器解析回调 ====================
void notifySensorCallback(NimBLERemoteCharacteristic *pChar, uint8_t *pData, size_t length, bool isNotify)
{
  uint16_t charUUID = pChar->getUUID().getNative()->u16.value;
  uint8_t flags = pData[0];

  if (charUUID == UUID_HRM_CHAR)
  {
    if (flags & 1)
      currentHR = pData[1] | (pData[2] << 8);
    else
      currentHR = pData[1];
    Serial.printf("[❤ HRM PARSED] 心率更新: %d bpm\n", currentHR);
  }
  else if (charUUID == UUID_CSC_CHAR)
  {
    int offset = 1;
    if (flags & 1)
      offset += 4;
    if (flags & 2)
    {
      uint16_t crankRevs = pData[offset] | (pData[offset + 1] << 8);
      uint16_t crankTime = pData[offset + 2] | (pData[offset + 3] << 8);
      if (lastCrankTime != 0 && crankTime != lastCrankTime)
      {
        uint16_t timeDiff = crankTime - lastCrankTime;
        uint16_t revsDiff = crankRevs - lastCrankRevs;
        currentCadence = (revsDiff * 60 * 1024) / timeDiff;
        lastCrankSysTime = millis();
        Serial.printf("[🚴 CSC PARSED] 踏频更新: %d rpm\n", currentCadence);
      }
      lastCrankRevs = crankRevs;
      lastCrankTime = crankTime;
    }
  }
}

// ==================== 客户端连接生命周期回调 ====================
class ClientCallbacks : public NimBLEClientCallbacks
{
  void onConnect(NimBLEClient *pClient) override
  {
    Serial.printf("[✅ CLIENT CONNECTED] 已连接设备: %s\n", pClient->getPeerAddress().toString().c_str());
  }
  void onDisconnect(NimBLEClient *pClient) override
  {
    Serial.printf("[❌ CLIENT DISCONNECTED] 设备断开: %s\n", pClient->getPeerAddress().toString().c_str());
    for (auto it = activeClients.begin(); it != activeClients.end();)
    {
      if (*it == pClient)
        it = activeClients.erase(it);
      else
        ++it;
    }
    if (activeClients.empty())
    {
      currentHR = 0;
      currentCadence = 0;
      if (isSensorMode)
      {
        FastLED.clear();
        FastLED.show();
      }
    }
  }
};
ClientCallbacks clientCB;

// ==================== 连接外设 ====================
void connectToSensor(NimBLEAddress addr)
{
  NimBLEClient *pClient = NimBLEDevice::createClient();
  pClient->setClientCallbacks(&clientCB, false);
  pClient->setConnectTimeout(10);
  pClient->setConnectionParams(12, 12, 0, 51);

  if (!pClient->connect(addr))
  {
    NimBLEDevice::deleteClient(pClient);
    return;
  }
  activeClients.push_back(pClient);

  isSensorMode = true;
  isClockMode = false;
  isTimerMode = false;
  isCountdownMode = false;

  NimBLERemoteService *pSvcHRM = pClient->getService(NimBLEUUID(UUID_HRM_SVC));
  if (pSvcHRM)
  {
    NimBLERemoteCharacteristic *pChr = pSvcHRM->getCharacteristic(NimBLEUUID(UUID_HRM_CHAR));
    if (pChr && pChr->canNotify())
      pChr->subscribe(true, notifySensorCallback);
  }

  NimBLERemoteService *pSvcCSC = pClient->getService(NimBLEUUID(UUID_CSC_SVC));
  if (pSvcCSC)
  {
    NimBLERemoteCharacteristic *pChr = pSvcCSC->getCharacteristic(NimBLEUUID(UUID_CSC_CHAR));
    if (pChr && pChr->canNotify())
      pChr->subscribe(true, notifySensorCallback);
  }
}

void disconnectFromSensor(NimBLEAddress addr)
{
  for (NimBLEClient *pClient : activeClients)
  {
    if (pClient->getPeerAddress() == addr)
    {
      pClient->disconnect();
      return;
    }
  }
}

// ==================== 代理扫描回调 ====================
class ScanCallbacks : public NimBLEAdvertisedDeviceCallbacks
{
  void onResult(NimBLEAdvertisedDevice *device)
  {
    if (device->haveName() && device->getName().length() > 0)
    {
      uint8_t type = 0;
      if (device->isAdvertisingService(NimBLEUUID(UUID_HRM_SVC)))
        type = 1;
      else if (device->isAdvertisingService(NimBLEUUID(UUID_CSC_SVC)))
        type = 2;

      string macStr = device->getAddress().toString();
      string name = device->getName();
      vector<uint8_t> payload = {0x05, type, device->getAddress().getType(), (uint8_t)macStr.length()};
      for (char c : macStr)
        payload.push_back(c);
      for (char c : name)
        payload.push_back(c);
      pTxCharacteristic->setValue(payload.data(), payload.size());
      pTxCharacteristic->notify();
    }
  }
};
ScanCallbacks scanCB;

// ==================== 接收 Web 指令 ====================
class RxCallbacks : public NimBLECharacteristicCallbacks
{
  void onWrite(NimBLECharacteristic *pChar)
  {
    string rxValue = pChar->getValue();
    if (rxValue.length() == 0)
      return;

    uint8_t cmd = (uint8_t)rxValue[0];

    if (cmd == 0x01 && rxValue.length() >= 2)
    {
      FastLED.setBrightness((uint8_t)rxValue[1]);
      FastLED.show();
    }
    else if (cmd == 0x02 && rxValue.length() >= 4)
    {
      fill_solid(leds, NUM_LEDS, CRGB(rxValue[1], rxValue[2], rxValue[3]));
      FastLED.show();
      isSensorMode = isClockMode = isTimerMode = isCountdownMode = false;
    }
    else if (cmd == 0x03)
    {
      FastLED.clear();
      FastLED.show();
      isSensorMode = isClockMode = isTimerMode = isCountdownMode = false;
    }
    else if (cmd == 0x04 && rxValue.length() >= 5)
    {
      uint32_t ts = ((uint32_t)rxValue[1] << 24) | ((uint32_t)rxValue[2] << 16) | ((uint32_t)rxValue[3] << 8) | (uint32_t)rxValue[4];
      struct timeval tv = {.tv_sec = (time_t)ts, .tv_usec = 0};
      settimeofday(&tv, NULL);
      isClockMode = true;
      isSensorMode = false;
      isTimerMode = false;
      isCountdownMode = false;
    }
    else if (cmd == 0x05)
      pendingCmd = 5;
    else if (cmd == 0x06 && rxValue.length() > 2)
    {
      pendingAddr = NimBLEAddress(rxValue.substr(2), rxValue[1]);
      pendingCmd = 6;
    }
    else if (cmd == 0x07 && rxValue.length() > 2)
    {
      pendingAddr = NimBLEAddress(rxValue.substr(2), rxValue[1]);
      pendingCmd = 7;
    }
    else if (cmd == 0x08)
    {
      isSensorMode = true;
      isClockMode = false;
      isTimerMode = false;
      isCountdownMode = false;
    }
    else if (cmd == 0x09)
    {
      isTimerMode = true;
      isSensorMode = false;
      isClockMode = false;
      isCountdownMode = false;
    }
    else if (cmd == 0x0A && rxValue.length() >= 2)
    { // 正向秒表控制
      uint8_t action = rxValue[1];
      if (action == 0x01 && !isTimerRunning)
      {
        timerStartTime = millis();
        isTimerRunning = true;
        isTimerMode = true;
        isSensorMode = isClockMode = isCountdownMode = false;
      }
      else if (action == 0x00 && isTimerRunning)
      {
        timerElapsed += millis() - timerStartTime;
        isTimerRunning = false;
      }
      else if (action == 0x02)
      {
        // 修复：重置归零后立即停止
        timerElapsed = 0;
        isTimerRunning = false;
      }
    }
    else if (cmd == 0x0B)
    {
      isCountdownMode = true;
      isSensorMode = isClockMode = isTimerMode = false;
    }
    else if (cmd == 0x0C && rxValue.length() >= 2)
    { // 设定倒计时长
      uint8_t mins = rxValue[1];
      countdownTotalSeconds = mins * 60;
      countdownRemaining = countdownTotalSeconds;
      isCountdownRunning = false;
      isCountdownFinished = false;
      isCountdownMode = true;
      isSensorMode = isClockMode = isTimerMode = false;
      Serial.printf("[⏳ CDOWN] 倒计时设定: %d 分钟\n", mins);
    }
    else if (cmd == 0x0D && rxValue.length() >= 2)
    { // 倒计时动作控制
      uint8_t action = rxValue[1];
      if (action == 0x01 && !isCountdownRunning && countdownRemaining > 0)
      { // 开始
        countdownStartSysTime = millis();
        isCountdownRunning = true;
        isCountdownFinished = false;
        isCountdownMode = true;
        isSensorMode = isClockMode = isTimerMode = false;
        Serial.println("[▶️ CDOWN] 倒计时开始");
      }
      else if (action == 0x00 && isCountdownRunning)
      { // 暂停
        uint32_t elapsed = (millis() - countdownStartSysTime) / 1000;
        countdownRemaining = (elapsed < countdownRemaining) ? (countdownRemaining - elapsed) : 0;
        isCountdownRunning = false;
        Serial.println("[⏸️ CDOWN] 倒计时暂停");
      }
      else if (action == 0x02)
      { // 重置
        countdownRemaining = countdownTotalSeconds;
        isCountdownRunning = false;
        isCountdownFinished = false;
        Serial.println("[🔄 CDOWN] 倒计时重置暂停");
      }
    }
  }
};

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n====================================");
  Serial.println("🚀 Pixel-Box BLE Core (Countdown Ver)");
  Serial.println("====================================");

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();

  NimBLEDevice::init(BLE_DEVICE_NAME);
  pServer = NimBLEDevice::createServer();
  NimBLEService *pService = pServer->createService(SERVICE_UUID);

  pRxCharacteristic = pService->createCharacteristic(CHAR_RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  pRxCharacteristic->setCallbacks(new RxCallbacks());
  pTxCharacteristic = pService->createCharacteristic(CHAR_TX_UUID, NIMBLE_PROPERTY::NOTIFY);

  pService->start();
  NimBLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID);
  NimBLEDevice::getAdvertising()->start();
}

void loop()
{
  if (pendingCmd != 0)
  {
    int cmd = pendingCmd;
    pendingCmd = 0;
    if (cmd == 5)
    {
      NimBLEScan *pScan = NimBLEDevice::getScan();
      pScan->setAdvertisedDeviceCallbacks(&scanCB);
      pScan->setActiveScan(true);
      pScan->start(5, false);
      pScan->clearResults();
    }
    else if (cmd == 6)
      connectToSensor(pendingAddr);
    else if (cmd == 7)
      disconnectFromSensor(pendingAddr);
  }

  if (millis() - lastCrankSysTime > 3000 && currentCadence != 0)
    currentCadence = 0;

  // 全局守护: 计算倒计时是否在后台归零
  if (isCountdownRunning)
  {
    uint32_t elapsed = (millis() - countdownStartSysTime) / 1000;
    if (elapsed >= countdownRemaining)
    {
      countdownRemaining = 0;
      isCountdownRunning = false;
      isCountdownFinished = true;
      countdownFinishSysTime = millis();
      // 强制唤醒/强制切屏至倒计时提醒界面
      isCountdownMode = true;
      isSensorMode = isClockMode = isTimerMode = false;
      Serial.println("[⏰ CDOWN] 倒计时触发! 强行提醒");
    }
  }

  // --- 画面渲染流分发 ---
  if (isClockMode)
  {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate >= 500)
    {
      lastUpdate = millis();
      time_t now;
      struct tm timeinfo;
      time(&now);
      localtime_r(&now, &timeinfo);

      FastLED.clear();
      CRGB color = CRGB(0, 180, 255);
      drawDigit(0, 1, timeinfo.tm_hour / 10, color);
      drawDigit(4, 1, timeinfo.tm_hour % 10, color);
      if (timeinfo.tm_sec % 2 == 0)
      {
        drawPixel(7, 2, color);
        drawPixel(7, 4, color);
      }
      drawDigit(9, 1, timeinfo.tm_min / 10, color);
      drawDigit(13, 1, timeinfo.tm_min % 10, color);
      FastLED.show();
    }
  }
  else if (isSensorMode)
  {
    static unsigned long lastToggle = 0;
    static bool showHR = true;
    bool hasHR = (currentHR > 0);
    bool hasCad = (currentCadence > 0 || !activeClients.empty());
    if (millis() - lastToggle > 3000)
    {
      showHR = !showHR;
      lastToggle = millis();
    }

    FastLED.clear();
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
      drawPixel(0, 2, CRGB::Red);
      drawPixel(1, 1, CRGB::Red);
      drawPixel(2, 2, CRGB::Red);
      drawPixel(1, 3, CRGB::Red);
      if (currentHR >= 100)
        drawDigit(4, 1, currentHR / 100, CRGB::Red);
      drawDigit(8, 1, (currentHR / 10) % 10, CRGB::Red);
      drawDigit(12, 1, currentHR % 10, CRGB::Red);
    }
    else
    {
    RENDER_CAD:
      CRGB cadCol = CRGB(0, 255, 128);
      drawPixel(0, 6, cadCol);
      drawPixel(1, 5, cadCol);
      drawPixel(0, 4, cadCol);
      if (currentCadence >= 100)
      {
        drawDigit(3, 1, (currentCadence / 100) % 10, cadCol);
        drawDigit(7, 1, (currentCadence / 10) % 10, cadCol);
        drawDigit(11, 1, currentCadence % 10, cadCol);
      }
      else if (currentCadence >= 10)
      {
        drawDigit(5, 1, (currentCadence / 10) % 10, cadCol);
        drawDigit(9, 1, currentCadence % 10, cadCol);
      }
      else
      {
        drawDigit(7, 1, currentCadence % 10, cadCol);
      }
    }
    FastLED.show();
    delay(100);
  }
  else if (isTimerMode)
  {
    static unsigned long lastTimerUpdate = 0;
    if (millis() - lastTimerUpdate >= 100)
    {
      lastTimerUpdate = millis();
      unsigned long currentMs = timerElapsed;
      if (isTimerRunning)
        currentMs += (millis() - timerStartTime);
      uint16_t totalSeconds = currentMs / 1000;
      uint8_t mins = (totalSeconds / 60) % 100;
      uint8_t secs = totalSeconds % 60;

      FastLED.clear();
      CRGB tColor = CRGB(255, 150, 0); // 橙色
      drawDigit(0, 1, mins / 10, tColor);
      drawDigit(4, 1, mins % 10, tColor);
      if (!isTimerRunning || (millis() / 500) % 2 == 0)
      {
        drawPixel(7, 2, tColor);
        drawPixel(7, 4, tColor);
      }
      drawDigit(9, 1, secs / 10, tColor);
      drawDigit(13, 1, secs % 10, tColor);
      FastLED.show();
    }
  }
  else if (isCountdownMode)
  {
    static unsigned long lastCDUpdate = 0;
    if (millis() - lastCDUpdate >= 100)
    {
      lastCDUpdate = millis();

      uint32_t currentRem = countdownRemaining;
      if (isCountdownRunning)
      {
        uint32_t elapsed = (millis() - countdownStartSysTime) / 1000;
        currentRem = (elapsed < countdownRemaining) ? (countdownRemaining - elapsed) : 0;
      }

      FastLED.clear();
      CRGB cdColor = CRGB(180, 0, 255); // 赛博紫

      if (isCountdownFinished)
      {
        // 归零高频闪烁 10 秒
        if (millis() - countdownFinishSysTime <= 10000)
        {
          if ((millis() / 250) % 2 == 0)
          { // 250ms 快闪
            drawDigit(0, 1, 0, cdColor);
            drawDigit(4, 1, 0, cdColor);
            drawPixel(7, 2, cdColor);
            drawPixel(7, 4, cdColor);
            drawDigit(9, 1, 0, cdColor);
            drawDigit(13, 1, 0, cdColor);
          }
        }
        else
        {
          // 闪烁结束常亮 00:00
          drawDigit(0, 1, 0, cdColor);
          drawDigit(4, 1, 0, cdColor);
          drawPixel(7, 2, cdColor);
          drawPixel(7, 4, cdColor);
          drawDigit(9, 1, 0, cdColor);
          drawDigit(13, 1, 0, cdColor);
        }
      }
      else
      {
        uint8_t mins = (currentRem / 60) % 100;
        uint8_t secs = currentRem % 60;
        drawDigit(0, 1, mins / 10, cdColor);
        drawDigit(4, 1, mins % 10, cdColor);
        if (!isCountdownRunning || (millis() / 500) % 2 == 0)
        {
          drawPixel(7, 2, cdColor);
          drawPixel(7, 4, cdColor);
        }
        drawDigit(9, 1, secs / 10, cdColor);
        drawDigit(13, 1, secs % 10, cdColor);
      }
      FastLED.show();
    }
  }
  delay(10);
}