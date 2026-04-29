#include <Arduino.h>
#include <FastLED.h>
#include <NimBLEDevice.h>
#include <sys/time.h>
#include "Config.h" // 严格保持 Config.h 作为单一真值源

// ==================== 全局状态与句柄 ====================
CRGB leds[NUM_LEDS];
NimBLEServer *pServer = nullptr;
NimBLECharacteristic *pRxCharacteristic = nullptr;
NimBLECharacteristic *pTxCharacteristic = nullptr;

bool isClockMode = false;
bool isSensorMode = false;

// 传感器数据缓存
uint8_t currentHR = 0;
uint16_t currentCadence = 0;
uint16_t lastCrankRevs = 0;
uint16_t lastCrankTime = 0;
unsigned long lastCrankSysTime = 0; // 用于计算踏频静止超时

// 已连接的外设客户端列表
vector<NimBLEClient *> activeClients;

// --- 异步任务解耦变量 ---
volatile int pendingCmd = 0; // 0=无, 5=扫描, 6=连接, 7=断开
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
      // 只有曲柄时间戳发生变化，才代表真实踩踏发生
      if (lastCrankTime != 0 && crankTime != lastCrankTime)
      {
        uint16_t timeDiff = crankTime - lastCrankTime;
        uint16_t revsDiff = crankRevs - lastCrankRevs;
        currentCadence = (revsDiff * 60 * 1024) / timeDiff;
        lastCrankSysTime = millis(); // 记录最新一次有效踩踏的系统时间
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
    // 保护机制：如果所有传感器都断开了，自动清屏并重置数据
    if (activeClients.empty())
    {
      Serial.println("[🧹 AUTO CLEAN] 所有外设已断开，自动清理屏幕残留");
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
  Serial.printf("\n[🔗 CONNECT INIT] 正在与 MAC: %s 建立物理链路...\n", addr.toString().c_str());
  NimBLEClient *pClient = NimBLEDevice::createClient();
  pClient->setClientCallbacks(&clientCB, false);

  pClient->setConnectTimeout(10);
  pClient->setConnectionParams(12, 12, 0, 51);

  if (!pClient->connect(addr))
  {
    Serial.printf("[⚠️ EXCEPTION] 连接失败! MAC: %s\n", addr.toString().c_str());
    NimBLEDevice::deleteClient(pClient);
    return;
  }

  activeClients.push_back(pClient);
  isClockMode = false;
  isSensorMode = true; // 只要连接成功就强制切换到数据展示面板

  NimBLERemoteService *pSvcHRM = pClient->getService(NimBLEUUID(UUID_HRM_SVC));
  if (pSvcHRM)
  {
    NimBLERemoteCharacteristic *pChr = pSvcHRM->getCharacteristic(NimBLEUUID(UUID_HRM_CHAR));
    if (pChr && pChr->canNotify())
    {
      pChr->subscribe(true, notifySensorCallback);
      Serial.println("[✅ SUBSCRIBE] 订阅 HRM 数据流成功");
    }
  }

  NimBLERemoteService *pSvcCSC = pClient->getService(NimBLEUUID(UUID_CSC_SVC));
  if (pSvcCSC)
  {
    NimBLERemoteCharacteristic *pChr = pSvcCSC->getCharacteristic(NimBLEUUID(UUID_CSC_CHAR));
    if (pChr && pChr->canNotify())
    {
      pChr->subscribe(true, notifySensorCallback);
      Serial.println("[✅ SUBSCRIBE] 订阅 CSC 数据流成功");
    }
  }
}

void disconnectFromSensor(NimBLEAddress addr)
{
  Serial.printf("\n[✂️ DISCONNECT INIT] 执行强制断开 MAC: %s\n", addr.toString().c_str());
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

      vector<uint8_t> payload;
      payload.push_back(0x05);
      payload.push_back(type);
      payload.push_back(device->getAddress().getType());
      payload.push_back(macStr.length());
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
    Serial.printf("\n[🌐 WEB->ESP] 收到报文 CMD: 0x%02X, 长度: %d bytes\n", cmd, rxValue.length());

    if (cmd == 0x01 && rxValue.length() >= 2)
    {
      // 0x01: 设定亮度并立刻生效
      FastLED.setBrightness((uint8_t)rxValue[1]);
      FastLED.show();
      Serial.printf("[🔆 UI] 亮度调整为: %d\n", (uint8_t)rxValue[1]);
    }
    else if (cmd == 0x02 && rxValue.length() >= 4)
    {
      fill_solid(leds, NUM_LEDS, CRGB(rxValue[1], rxValue[2], rxValue[3]));
      FastLED.show();
      isSensorMode = isClockMode = false;
    }
    else if (cmd == 0x03)
    {
      // 0x03: 强制息屏 (Clear)
      FastLED.clear();
      FastLED.show();
      isSensorMode = isClockMode = false;
      Serial.println("[📺 UI] 屏幕已强制熄灭");
    }
    else if (cmd == 0x04 && rxValue.length() >= 5)
    {
      // 0x04: 强制切换至时间面
      uint32_t ts = ((uint32_t)rxValue[1] << 24) | ((uint32_t)rxValue[2] << 16) | ((uint32_t)rxValue[3] << 8) | (uint32_t)rxValue[4];
      struct timeval tv = {.tv_sec = (time_t)ts, .tv_usec = 0};
      settimeofday(&tv, NULL);
      isClockMode = true;
      isSensorMode = false;
      Serial.println("[🕒 UI] 切换至时钟模式");
    }
    else if (cmd == 0x05)
      pendingCmd = 5;
    else if (cmd == 0x06 && rxValue.length() > 2)
    {
      uint8_t addrType = rxValue[1];
      string macStr = rxValue.substr(2);
      pendingAddr = NimBLEAddress(macStr, addrType);
      pendingCmd = 6;
    }
    else if (cmd == 0x07 && rxValue.length() > 2)
    {
      uint8_t addrType = rxValue[1];
      string macStr = rxValue.substr(2);
      pendingAddr = NimBLEAddress(macStr, addrType);
      pendingCmd = 7;
    }
    else if (cmd == 0x08)
    {
      // 0x08: 强制切换至运动数据面
      isSensorMode = true;
      isClockMode = false;
      Serial.println("[🚴 UI] 切换至运动数据模式");
    }
  }
};

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n====================================");
  Serial.println("🚀 Pixel-Box BLE Core (Display Overhaul)");
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
  Serial.println("[📡 BLE] 网关广播启动，等待连接...");
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

  // --- 实时静止踏频清零逻辑 ---
  // 如果超过 3 秒没有收到新的曲柄时间戳，说明用户停止了踩踏
  if (millis() - lastCrankSysTime > 3000 && currentCadence != 0)
  {
    currentCadence = 0;
  }

  // --- 画面渲染流 ---
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

    // 如果只有一种设备有数据，固定显示该设备；如果两种都有，每 3 秒交替展示
    bool hasHR = (currentHR > 0);
    bool hasCad = (currentCadence > 0 || !activeClients.empty()); // 如果有连接但还没数据，默认显示 0 踏频

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
      drawPixel(1, 3, CRGB::Red); // 心跳Icon
      if (currentHR >= 100)
        drawDigit(4, 1, currentHR / 100, CRGB::Red);
      drawDigit(8, 1, (currentHR / 10) % 10, CRGB::Red);
      drawDigit(12, 1, currentHR % 10, CRGB::Red);
    }
    else
    {
    RENDER_CAD:
      CRGB cadCol = CRGB(0, 255, 128); // 踏频极光绿
      drawPixel(0, 6, cadCol);
      drawPixel(1, 5, cadCol);
      drawPixel(0, 4, cadCol); // 简易踏板 Icon

      // 居中渲染 1-3 位数字的踏频
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
        drawDigit(7, 1, currentCadence % 10, cadCol); // 只有 1 位数（包括0）居中
      }
    }

    FastLED.show();
    delay(100);
  }
  delay(10);
}