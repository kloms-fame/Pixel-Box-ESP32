#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <string>
#include <vector>

using namespace std;

// ==================== 硬件与渲染配置 ====================
#define LED_PIN 9
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 8
#define NUM_LEDS 128
#define BRIGHTNESS 30
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

// ==================== BLE 网关协议配置 ====================
#define BLE_DEVICE_NAME "Pixel-Box-Mini"
#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHAR_RX_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e" // Web -> ESP32
#define CHAR_TX_UUID "6e400004-b5a3-f393-e0a9-e50e24dcca9e" // ESP32 -> Web

// ==================== 骑行传感器服务 UUID ====================
#define UUID_HRM_SVC (uint16_t)0x180D
#define UUID_HRM_CHAR (uint16_t)0x2A37
#define UUID_CSC_SVC (uint16_t)0x1816
#define UUID_CSC_CHAR (uint16_t)0x2A5B

#endif // CONFIG_H