#pragma once
#include "Config.h"
#include <string>
#include <cstdint>

void WebGateway_Init();
void WebGateway_SendScanResult(uint8_t type, uint8_t addrType, std::string macStr, std::string name);
void WebGateway_BroadcastBasicState();
void WebGateway_BroadcastCdownConfig();