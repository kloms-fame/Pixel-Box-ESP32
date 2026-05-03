#include "VoiceInputMachine.h"
#include "VoiceAssistant.h"
#include "GlobalState.h"
#include "DisplayCore.h"
#include "WebGateway.h"

VoiceInputState VoiceInputMachine::currentState = V_IDLE;
uint8_t VoiceInputMachine::buffer[4] = {0};
uint8_t VoiceInputMachine::inputIndex = 0;
uint8_t VoiceInputMachine::targetAlarmIndex = 0;
unsigned long VoiceInputMachine::lastInputSysTime = 0;

void VoiceInputMachine::init()
{
    currentState = V_IDLE;
    inputIndex = 0;       // 建议加上
    targetAlarmIndex = 0; // 建议加上
}

void VoiceInputMachine::loop()
{
    if (currentState != V_IDLE && (millis() - lastInputSysTime > 8000))
    {
        abortInput(true);
    }
}

// 入口：用户说“定闹钟”
void VoiceInputMachine::startAlarmSelection()
{
    currentState = V_WAIT_INDEX;
    lastInputSysTime = millis();
    Serial.println("[🎙️ 状态机] 进入闹钟选择模式，请说出组号 (1-3)");
    // 这里不需要切屏，等选好了组再切
}

void VoiceInputMachine::startCountdownInput()
{
    AppState.currentMode = MODE_COUNTDOWN;
    currentState = V_WAIT_CDOWN;
    inputIndex = 0;
    lastInputSysTime = millis();
}

void VoiceInputMachine::feedDigit(uint8_t digit)
{
    if (currentState == V_IDLE)
        return;
    lastInputSysTime = millis();
    VoiceAssistant_Beep(50);

    // 情况 A：正在选闹钟组号
    if (currentState == V_WAIT_INDEX)
    {
        if (digit >= 1 && digit <= 3)
        {
            targetAlarmIndex = digit - 1; // 转换为 0, 1, 2
            AppState.alarmDisplayIndex = targetAlarmIndex;
            AppState.currentMode = MODE_ALARM;

            // 选好了，进入录入时间状态
            currentState = V_WAIT_ALARM;
            inputIndex = 0;
            // 反馈给 SU-03T 播报：“好的，开始设定第 X 组闹钟，请说四位数字”
            VoiceAssistant_Send1(0xA5, digit);
            Serial.printf("[🎙️ 状态机] 已选中闹钟组 %d，等待录入时间\n", digit);
        }
        else
        {
            // 输入了无效组号 (比如数字5)，提示错误
            VoiceAssistant_Send0(0xA6);
        }
        return;
    }

    // 情况 B：正在录入数字
    buffer[inputIndex++] = digit;
    if (currentState == V_WAIT_ALARM && inputIndex == 4)
    {
        processAlarm();
    }
    else if (currentState == V_WAIT_CDOWN && inputIndex == 2)
    {
        processCountdown();
    }
}

// 中止输入
void VoiceInputMachine::abortInput(bool isTimeout)
{
    if (currentState != V_IDLE)
    {
        currentState = V_IDLE;
        inputIndex = 0;
        if (isTimeout)
        {
            // 【新增】如果是超时，发送 0x97 给 SU-03T，让它语音播报并重置变量锁
            VoiceAssistant_Send0(0x97);
            Serial.println("[🎙️ 状态机] ⏳ 输入超时 (8秒)，已自动中止");
        }
        else
        {
            Serial.println("[🎙️ 状态机] 🛑 输入已手动中止");
        }
    }
}

// 处理闹钟逻辑
void VoiceInputMachine::processAlarm()
{
    uint8_t hh = buffer[0] * 10 + buffer[1];
    uint8_t mm = buffer[2] * 10 + buffer[3];

    if (hh < 24 && mm < 60)
    {
        uint8_t idx = AppState.alarmDisplayIndex;
        AppState.saveAlarm(idx, true, hh, mm);
        WebGateway_BroadcastAlarmState(idx);

        AppState.currentMode = MODE_ALARM;

        VoiceAssistant_Send2(0xA2, hh, mm);
        Serial.printf("[⏰ 闹钟] 设定成功: %02d:%02d\n", hh, mm);
    }
    else
    {
        VoiceAssistant_Send0(0xA3);
        Serial.println("[⏰ 闹钟] 设定失败: 格式错误");
    }
    currentState = V_IDLE;
}

// 处理倒计时逻辑
void VoiceInputMachine::processCountdown()
{
    uint8_t mm = buffer[0] * 10 + buffer[1];

    if (mm > 0 && mm <= 99)
    {
        AppState.countdownTotalSeconds = mm * 60;
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
        AppState.isCountdownRunning = false;

        AppState.currentMode = MODE_COUNTDOWN;

        WebGateway_BroadcastCdownConfig();

        VoiceAssistant_Send1(0xC2, mm);
        Serial.printf("[⏳ 倒数] 设定成功: %d 分钟\n", mm);
    }
    else
    {
        VoiceAssistant_Send0(0xC3);
        Serial.println("[⏳ 倒数] 设定失败: 时长越界");
    }
    currentState = V_IDLE;
}