#ifndef VOICE_ASSISTANT_H
#define VOICE_ASSISTANT_H

#include <Arduino.h>

void VoiceAssistant_Init();
void VoiceAssistant_Loop();

// 暴露底层协议发送器给状态机使用
void VoiceAssistant_Send0(uint8_t msgId);
void VoiceAssistant_Send1(uint8_t msgId, uint8_t v1);
void VoiceAssistant_Send2(uint8_t msgId, uint8_t v1, uint8_t v2);
void VoiceAssistant_Send4(uint8_t msgId, uint8_t v1, uint8_t v2, uint8_t v3, uint8_t v4);

// 暴露蜂鸣器反馈给状态机使用
void VoiceAssistant_Beep(uint16_t duration_ms = 50);

#endif