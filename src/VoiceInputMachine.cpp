#include "VoiceInputMachine.h"
#include "VoiceAssistant.h"
#include "GlobalState.h"
#include "DisplayCore.h"
#include "WebGateway.h"

VoiceInputState VoiceInputMachine::currentState = V_IDLE;
uint8_t VoiceInputMachine::buffer[4] = {0};
uint8_t VoiceInputMachine::inputIndex = 0;

void VoiceInputMachine::init()
{
    currentState = V_IDLE;
    inputIndex = 0;
}

// 开启闹钟输入模式
void VoiceInputMachine::startAlarmInput()
{
    currentState = V_WAIT_ALARM;
    inputIndex = 0;
    Serial.println("[🎙️ 状态机] 进入等待输入闹钟模式 (需4位数字)");
}

// 开启倒数输入模式
void VoiceInputMachine::startCountdownInput()
{
    currentState = V_WAIT_CDOWN;
    inputIndex = 0;
    Serial.println("[🎙️ 状态机] 进入等待输入倒数模式 (需2位数字)");
}

// 喂入语音数字
void VoiceInputMachine::feedDigit(uint8_t digit)
{
    if (currentState == V_IDLE)
        return; // 空闲时忽略数字指令

    // 每收到一个数字，滴一声作为确认反馈
    VoiceAssistant_Beep(50);

    buffer[inputIndex++] = digit;
    Serial.printf("[🎙️ 状态机] 收到数字: %d, 当前进度: %d\n", digit, inputIndex);

    // 闹钟需要 4 位数字
    if (currentState == V_WAIT_ALARM && inputIndex == 4)
    {
        processAlarm();
    }
    // 倒计时需要 2 位数字
    else if (currentState == V_WAIT_CDOWN && inputIndex == 2)
    {
        processCountdown();
    }
}

// 强行中止
void VoiceInputMachine::abortInput()
{
    if (currentState != V_IDLE)
    {
        currentState = V_IDLE;
        inputIndex = 0;
        Serial.println("[🎙️ 状态机] 输入已中止并复位");
    }
}

// 处理闹钟逻辑
void VoiceInputMachine::processAlarm()
{
    uint8_t hh = buffer[0] * 10 + buffer[1];
    uint8_t mm = buffer[2] * 10 + buffer[3];

    if (hh < 24 && mm < 60)
    {
        // 数据合法，保存至当前选中的闹钟组
        uint8_t idx = AppState.alarmDisplayIndex;
        AppState.saveAlarm(idx, true, hh, mm);
        WebGateway_BroadcastAlarmState(idx);

        AppState.currentMode = MODE_ALARM;
        Display_Clear();
        Display_Show();

        // 成功反馈: AA 55 A2 [HH] [MM] 55 AA
        VoiceAssistant_Send2(0xA2, hh, mm);
        Serial.printf("[⏰ 闹钟] 语音设定成功: %02d:%02d\n", hh, mm);
    }
    else
    {
        // 数据非法打回: AA 55 A3 55 AA
        VoiceAssistant_Send0(0xA3);
        Serial.println("[⏰ 闹钟] 设定失败: 时间格式错误");
    }
    currentState = V_IDLE; // 处理完归位
}

// 处理倒计时逻辑
void VoiceInputMachine::processCountdown()
{
    uint8_t mm = buffer[0] * 10 + buffer[1];

    if (mm > 0 && mm <= 99)
    {
        // 数据合法，启动倒计时
        AppState.countdownTotalSeconds = mm * 60;
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
        AppState.isCountdownRunning = false;

        AppState.currentMode = MODE_COUNTDOWN;
        Display_Clear();
        Display_Show();
        WebGateway_BroadcastCdownConfig();

        // 成功反馈: AA 55 C2 [MM] 55 AA
        VoiceAssistant_Send1(0xC2, mm);
        Serial.printf("[⏳ 倒数] 语音设定成功: %d 分钟\n", mm);
    }
    else
    {
        // 数据非法打回: AA 55 C3 55 AA
        VoiceAssistant_Send0(0xC3);
        Serial.println("[⏳ 倒数] 设定失败: 时长越界");
    }
    currentState = V_IDLE; // 处理完归位
}