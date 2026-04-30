#pragma once
#include "Config.h"
#include <string>
#include <cstdint>

void WebGateway_Init();
void WebGateway_SendScanResult(uint8_t type, uint8_t addrType, std::string macStr, std::string name);

// 暴露物理硬件反向推流给Web的方法
void WebGateway_BroadcastBasicState();
void WebGateway_BroadcastCdownConfig();
void WebGateway_BroadcastAlarmState(uint8_t idx);
void WebGateway_BroadcastSavedDevices(); // 【新增】广播 NVS 已存设备