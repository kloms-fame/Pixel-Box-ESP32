#pragma once
#include <Arduino.h>

// 定义语音输入状态机的三大状态
enum VoiceInputState
{
    V_IDLE = 0,   // 空闲状态
    V_WAIT_ALARM, // 等待输入四位闹钟 (HHMM)
    V_WAIT_CDOWN  // 等待输入两位倒计时 (MM)
};

class VoiceInputMachine
{
public:
    static VoiceInputState currentState;

    static void init();
    static void startAlarmInput();        // 触发定闹钟
    static void startCountdownInput();    // 触发自定义倒数
    static void feedDigit(uint8_t digit); // 喂入数字 (0-9)
    static void abortInput();             // 中止/超时退出

private:
    static uint8_t buffer[4]; // 最多装4个数字
    static uint8_t inputIndex;

    static void processAlarm();
    static void processCountdown();
};