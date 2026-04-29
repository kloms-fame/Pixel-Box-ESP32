#pragma once
#include "Config.h"
#include <NimBLEDevice.h>
#include <functional>
#include <string>

// 【修复】直接使用 std::string 传递 MAC，彻底避免底层字节数组传递时的端序 (Endianness) 翻转问题
typedef std::function<void(uint8_t type, uint8_t addrType, std::string macStr, std::string name)> ScanCallbackFunc;

void SensorHub_StartScan(ScanCallbackFunc cb);
void SensorHub_Connect(NimBLEAddress addr);
void SensorHub_Disconnect(NimBLEAddress addr);
int SensorHub_GetActiveClientCount();