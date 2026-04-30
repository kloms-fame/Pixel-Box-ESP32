#pragma once
#include "Config.h"
#include <NimBLEDevice.h>
#include <functional>
#include <string>

typedef std::function<void(uint8_t type, uint8_t addrType, std::string macStr, std::string name)> ScanCallbackFunc;

void SensorHub_StartScan(ScanCallbackFunc cb);
void SensorHub_Connect(NimBLEAddress addr);
void SensorHub_Disconnect(NimBLEAddress addr);
void SensorHub_DisconnectAll(); // 新增：断开所有设备
int SensorHub_GetActiveClientCount();
void SensorHub_TriggerAutoReconnect(bool force = false); // 新增：强制扫描重连