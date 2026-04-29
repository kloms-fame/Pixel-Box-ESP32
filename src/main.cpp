#include <Arduino.h>
#include <FastLED.h>
#include <NimBLEDevice.h>
#include <sys/time.h> // 用于 ESP32 内部软时钟

// ==================== 硬件配置 ====================
#define LED_PIN 9
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 8
#define NUM_LEDS 128
#define BRIGHTNESS 30
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

// ==================== BLE 配置 ====================
#define BLE_DEVICE_NAME "Pixel-Box-Mini"
#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHAR_RX_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

// ==================== 全局变量 ====================
CRGB leds[NUM_LEDS];
uint8_t currentBrightness = BRIGHTNESS;
NimBLEServer *pServer = nullptr;
NimBLECharacteristic *pRxCharacteristic = nullptr;
bool deviceConnected = false;
bool isClockMode = false; // 是否处于时钟模式

// ==================== 3x5 字体字典 ====================
static const uint8_t digits[10][5] = {
    {0x07, 0x05, 0x05, 0x05, 0x07}, // 0
    {0x01, 0x01, 0x01, 0x01, 0x01}, // 1
    {0x07, 0x01, 0x07, 0x04, 0x07}, // 2
    {0x07, 0x01, 0x07, 0x01, 0x07}, // 3
    {0x05, 0x05, 0x07, 0x01, 0x01}, // 4
    {0x07, 0x04, 0x07, 0x01, 0x07}, // 5
    {0x07, 0x04, 0x07, 0x05, 0x07}, // 6
    {0x07, 0x01, 0x01, 0x01, 0x01}, // 7
    {0x07, 0x05, 0x07, 0x05, 0x07}, // 8
    {0x07, 0x05, 0x07, 0x01, 0x07}  // 9
};

// ==================== 坐标映射 ====================
uint16_t getIndex(uint8_t x, uint8_t y)
{
  if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT)
    return 0;
  if (x < 8)
    return y * 8 + x;
  else
    return 64 + y * 8 + (x - 8);
}

void drawPixelCoords(int x, int y, CRGB color)
{
  if (x < 0 || x >= MATRIX_WIDTH || y < 0 || y >= MATRIX_HEIGHT)
    return;
  leds[getIndex(x, y)] = color;
}

void drawDigit(int x, int y, int digit, CRGB color)
{
  if (digit < 0 || digit > 9)
    return;
  for (int row = 0; row < 5; row++)
  {
    for (int col = 0; col < 3; col++)
    {
      if (digits[digit][row] & (1 << (2 - col)))
      {
        drawPixelCoords(x + col, y + row, color);
      }
    }
  }
}

// ==================== BLE 回调类 ====================
class ServerCallbacks : public NimBLEServerCallbacks
{
  void onConnect(NimBLEServer *pServer)
  {
    deviceConnected = true;
    Serial.println("✅ BLE 已连接");
  }
  void onDisconnect(NimBLEServer *pServer)
  {
    deviceConnected = false;
    Serial.println("❌ BLE 已断开，重启广播");
    pServer->startAdvertising();
  }
};

class RxCallbacks : public NimBLECharacteristicCallbacks
{
  void onWrite(NimBLECharacteristic *pChar)
  {
    std::string rxValue = pChar->getValue();
    if (rxValue.length() == 0)
      return;

    uint8_t cmd = (uint8_t)rxValue[0];
    switch (cmd)
    {
    case 0x01: // 亮度调节
      if (rxValue.length() >= 2)
      {
        currentBrightness = (uint8_t)rxValue[1];
        FastLED.setBrightness(currentBrightness);
        if (!isClockMode)
          FastLED.show(); // 避免时钟模式下的闪烁
        Serial.printf("🔆 亮度: %d\n", currentBrightness);
      }
      break;
    case 0x02: // 全屏填充
      if (rxValue.length() >= 4)
      {
        isClockMode = false;
        uint8_t r = rxValue[1], g = rxValue[2], b = rxValue[3];
        fill_solid(leds, NUM_LEDS, CRGB(r, g, b));
        FastLED.show();
        Serial.printf("🎨 填充: %d,%d,%d\n", r, g, b);
      }
      break;
    case 0x03: // 清屏
      isClockMode = false;
      FastLED.clear();
      FastLED.show();
      Serial.println("🗑️ 清屏");
      break;
    case 0x04: // 🕒 时钟同步 [0x04, TS3, TS2, TS1, TS0]
      if (rxValue.length() >= 5)
      {
        // 重组 32 位时间戳
        uint32_t ts = ((uint32_t)rxValue[1] << 24) |
                      ((uint32_t)rxValue[2] << 16) |
                      ((uint32_t)rxValue[3] << 8) |
                      (uint32_t)rxValue[4];

        struct timeval tv;
        tv.tv_sec = ts;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);

        isClockMode = true;
        Serial.printf("🕒 时间同步成功，进入时钟模式，时间戳: %lu\n", ts);
      }
      break;
    }
  }
};

// ==================== 初始化 ====================
void initBLE()
{
  NimBLEDevice::init(BLE_DEVICE_NAME);
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());
  NimBLEService *pService = pServer->createService(SERVICE_UUID);
  pRxCharacteristic = pService->createCharacteristic(CHAR_RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  pRxCharacteristic->setCallbacks(new RxCallbacks());
  pService->start();
  NimBLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID);
  NimBLEDevice::getAdvertising()->start();
  Serial.println("📡 BLE 服务启动");
}

void setup()
{
  Serial.begin(115200);
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(currentBrightness);
  FastLED.clear();
  FastLED.show();
  initBLE();
}

void loop()
{
  if (isClockMode)
  {
    static unsigned long lastUpdate = 0;
    // 每 500ms 刷新一次以实现中间冒号的动态闪烁
    if (millis() - lastUpdate >= 500)
    {
      lastUpdate = millis();

      time_t now;
      struct tm timeinfo;
      time(&now);
      localtime_r(&now, &timeinfo); // 读取 ESP32 内部时间

      FastLED.clear();
      CRGB clockColor = CRGB(0, 255, 255); // 极简青色冷光

      // H1 (x=0~2), H2 (x=4~6)
      drawDigit(0, 1, timeinfo.tm_hour / 10, clockColor);
      drawDigit(4, 1, timeinfo.tm_hour % 10, clockColor);

      // 闪烁冒号 (x=7)
      if (timeinfo.tm_sec % 2 == 0)
      {
        drawPixelCoords(7, 2, clockColor);
        drawPixelCoords(7, 4, clockColor);
      }

      // M1 (x=9~11), M2 (x=13~15)
      drawDigit(9, 1, timeinfo.tm_min / 10, clockColor);
      drawDigit(13, 1, timeinfo.tm_min % 10, clockColor);

      FastLED.show();
    }
  }
  delay(10);
}