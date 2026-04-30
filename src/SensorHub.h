#pragma once
#include "Config.h"
#include <NimBLEDevice.h>
#include <functional>
#include <string>

typedef std::function<void(uint8_t type, uint8_t addrType, std::string macStr, std::string name)> ScanCallbackFunc;

void SensorHub_StartScan(ScanCallbackFunc cb);
void SensorHub_Connect(NimBLEAddress addr);
void SensorHub_Disconnect(NimBLEAddress addr);
void SensorHub_DisconnectAll();
int SensorHub_GetActiveClientCount();
void SensorHub_TriggerAutoReconnect(bool force = false, uint8_t targetType = 0);
void SensorHub_DisconnectType(uint8_t type);

// 【新增】智能探测当前是否有特定的设备在线
bool SensorHub_HasHRM();
bool SensorHub_HasCSC();