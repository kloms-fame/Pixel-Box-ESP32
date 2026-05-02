#pragma once
#include <Arduino.h>

enum VoiceInputState
{
    V_IDLE = 0,
    V_WAIT_INDEX, // 【新增】等待选择闹钟组号 (1位数字)
    V_WAIT_ALARM, // 等待输入四位时间 (HHMM)
    V_WAIT_CDOWN  // 等待输入两位倒计时 (MM)
};

class VoiceInputMachine
{
public:
    static VoiceInputState currentState;
    static void init();
    static void loop();

    static void startAlarmSelection(); // 【新增】入口：询问第几组
    static void startCountdownInput();
    static void feedDigit(uint8_t digit);
    static void abortInput(bool isTimeout = false);

private:
    static uint8_t buffer[4];
    static uint8_t inputIndex;
    static uint8_t targetAlarmIndex; // 暂存选中的闹钟组
    static unsigned long lastInputSysTime;

    static void processAlarm();
    static void processCountdown();
};