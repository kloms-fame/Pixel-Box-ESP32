#pragma once
#include <Arduino.h>

// 独立语音助手引擎
// 完全与主业务解耦，在后台静默处理串口通信与定时巡检任务

void VoiceAssistant_Init();
void VoiceAssistant_Loop();