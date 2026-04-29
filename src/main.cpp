#include <Arduino.h>
#include <FastLED.h>
#include <NimBLEDevice.h>

// ==================== 硬件配置 ====================
#define LED_PIN 9
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 8
#define NUM_LEDS 128
#define BRIGHTNESS 30
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

// ==================== BLE 配置（严格全小写 UUID） ====================
#define BLE_DEVICE_NAME "Pixel-Box-Mini"
#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e" // 全小写
#define CHAR_RX_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e" // 接收特征（手机→ESP32）

// ==================== 全局变量 ====================
CRGB leds[NUM_LEDS];
uint8_t currentBrightness = BRIGHTNESS;
NimBLEServer *pServer = nullptr;
NimBLECharacteristic *pRxCharacteristic = nullptr;
bool deviceConnected = false;

// ==================== 坐标映射（适配双 8x8 串联） ====================
uint16_t getIndex(uint8_t x, uint8_t y)
{
  if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT)
    return 0;
  if (x < 8)
    return y * 8 + x;
  else
    return 64 + y * 8 + (x - 8);
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

    Serial.print("📥 收到指令: ");
    for (uint8_t i = 0; i < rxValue.length(); i++)
      Serial.printf("%02X ", (uint8_t)rxValue[i]);
    Serial.println();

    uint8_t cmd = (uint8_t)rxValue[0];
    switch (cmd)
    {
    case 0x01: // 亮度调节 [0x01, 亮度值]
      if (rxValue.length() >= 2)
      {
        currentBrightness = (uint8_t)rxValue[1];
        FastLED.setBrightness(currentBrightness);
        FastLED.show();
        Serial.printf("🔆 亮度: %d\n", currentBrightness);
      }
      break;
    case 0x02: // 全屏填充 [0x02, R, G, B]
      if (rxValue.length() >= 4)
      {
        uint8_t r = (uint8_t)rxValue[1], g = (uint8_t)rxValue[2], b = (uint8_t)rxValue[3];
        fill_solid(leds, NUM_LEDS, CRGB(r, g, b));
        FastLED.show();
        Serial.printf("🎨 填充: R=%d, G=%d, B=%d\n", r, g, b);
      }
      break;
    case 0x03: // 清屏 [0x03]
      FastLED.clear();
      FastLED.show();
      Serial.println("🗑️ 清屏");
      break;
    }
  }
};

// ==================== BLE 初始化 ====================
void initBLE()
{
  NimBLEDevice::init(BLE_DEVICE_NAME);
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  NimBLEService *pService = pServer->createService(SERVICE_UUID);
  pRxCharacteristic = pService->createCharacteristic(CHAR_RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  pRxCharacteristic->setCallbacks(new RxCallbacks());

  pService->start();
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->start();
  Serial.println("📡 BLE 服务器已启动，设备名: " + String(BLE_DEVICE_NAME));
}

// ==================== 系统初始化 ====================
void setup()
{
  Serial.begin(115200);
  delay(100);
  Serial.println("\n🚀 Pixel-Box-Mini 启动");

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(currentBrightness);
  FastLED.clear();
  FastLED.show();
  Serial.println("💡 WS2812 初始化完成");

  initBLE();
}

void loop()
{
  delay(10);
}