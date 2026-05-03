#include "VoiceAssistant.h"
#include "EventBus.h"
#include "ModeManager.h"
#include "VoiceInputMachine.h" // 【新增】引入状态机
#include "GlobalState.h"
#include "SensorHub.h"
#include "WebGateway.h"
#include "TimeSync.h"
#include "DisplayCore.h"
#include <HardwareSerial.h>

#define PIN_VOICE_RX 2
#define PIN_VOICE_TX 3
#define PIN_BUZZER 0

HardwareSerial VoiceSerial(1);

// 变量保持不变
static bool metronomeEnabled = false;
static uint16_t metronomeBpm = 0;
static uint8_t hrZone = 0;
static uint32_t lastReportedSec = 99;
static bool isRiding = false;
static unsigned long rideStartSysTime = 0;
static float rideDistanceKm = 0.0;

// ==========================================
// 底层硬件管控
// ==========================================
void VoiceAssistant_Beep(uint16_t duration_ms)
{
    digitalWrite(PIN_BUZZER, HIGH);
    delay(duration_ms);
    digitalWrite(PIN_BUZZER, LOW);
}

void VoiceAssistant_Send0(uint8_t msgId)
{
    uint8_t buf[5] = {0xAA, 0x55, msgId, 0x55, 0xAA};
    VoiceSerial.write(buf, 5);
}
void VoiceAssistant_Send1(uint8_t msgId, uint8_t v1)
{
    uint8_t buf[6] = {0xAA, 0x55, msgId, v1, 0x55, 0xAA};
    VoiceSerial.write(buf, 6);
}
void VoiceAssistant_Send2(uint8_t msgId, uint8_t v1, uint8_t v2)
{
    uint8_t buf[7] = {0xAA, 0x55, msgId, v1, v2, 0x55, 0xAA};
    VoiceSerial.write(buf, 7);
}
void VoiceAssistant_Send4(uint8_t msgId, uint8_t v1, uint8_t v2, uint8_t v3, uint8_t v4)
{
    uint8_t buf[9] = {0xAA, 0x55, msgId, v1, v2, v3, v4, 0x55, 0xAA};
    VoiceSerial.write(buf, 9);
}

// ==========================================
// 路由解析器
// ==========================================
void ProcessVoiceCommand(uint8_t cmd, uint8_t param)
{
    Serial.printf("\n[🎙️ VOICE] 👉 指令匹配: CMD[0x%02X] PARAM[%d]\n", cmd, param);

    // 【新增】如果是 E0~E9，说明是用户念出的数字 0~9，直接喂给状态机！
    if (cmd >= 0xE0 && cmd <= 0xE9)
    {
        VoiceInputMachine::feedDigit(cmd - 0xE0);
        return;
    }

    switch (cmd)
    {
    // --- 【新增】多步输入唤醒指令 ---
    // --- 提取 param 参数，支持指定跳转 ---
    // --- 提取 param 参数，支持指定跳转 ---
    case 0xA1:
        VoiceInputMachine::startAlarmSelection();
        break;
    case 0xC1:
        VoiceInputMachine::startCountdownInput();
        break;
    case 0x98:
        VoiceInputMachine::abortInput(false);
        break;

    // ... (下方是你原本 V26 的所有 case 指令，完全保持原样，无需修改) ...
    case 0x0C:
    {
        EventMsg eSet = {EVT_VOICE, ACT_CDOWN_SET, (uint32_t)(param * 60)};
        Event_Push(eSet);
        EventMsg eMode = {EVT_VOICE, ACT_MODE_SWITCH, MODE_COUNTDOWN};
        Event_Push(eMode);
        VoiceAssistant_Send1(0x01, param);
        break;
    }
    case 0x0D:
    {
        if (param == 1)
        {
            EventMsg e = {EVT_VOICE, ACT_CDOWN_START, 0};
            Event_Push(e);
        }
        else if (param == 0)
        {
            EventMsg e = {EVT_VOICE, ACT_CDOWN_PAUSE, 0};
            Event_Push(e);
        }
        else
        {
            EventMsg e = {EVT_VOICE, ACT_CDOWN_RESET, 0};
            Event_Push(e);
        }
        // ✅ 模式切换也通过事件（或者因为上面的事件处理里已经包含了模式切换，这里可以省略）
        EventMsg eMode = {EVT_VOICE, ACT_MODE_SWITCH, MODE_COUNTDOWN};
        Event_Push(eMode);
        break;
    }
    case 0x0A:
    {
        if (param == 1)
        {
            EventMsg e = {EVT_VOICE, ACT_TIMER_START, 0};
            Event_Push(e);
        }
        else if (param == 0)
        {
            EventMsg e = {EVT_VOICE, ACT_TIMER_PAUSE, 0};
            Event_Push(e);
        }
        else
        {
            EventMsg e = {EVT_VOICE, ACT_TIMER_RESET, 0};
            Event_Push(e);
        }
        EventMsg eMode = {EVT_VOICE, ACT_MODE_SWITCH, MODE_TIMER};
        Event_Push(eMode);
        break;
    }
    case 0x34:
        metronomeEnabled = true;
        metronomeBpm = param;
        break;
    case 0x35:
        metronomeEnabled = false;
        break;
    case 0x36:
        hrZone = 1;
        break;
    case 0x37:
        hrZone = 2;
        break;
    case 0x38:
        hrZone = 0;
        break;
    case 0x39:
        VoiceAssistant_Send2(0x03, AppState.currentHR, AppState.currentCadence);
        break;
    case 0x3A:
    {
        time_t n;
        struct tm ti;
        time(&n);
        localtime_r(&n, &ti);
        VoiceAssistant_Send2(0x02, ti.tm_hour, ti.tm_min);
    }
    break;
    case 0x41:
        VoiceAssistant_Send1(0x05, AppState.currentCadence);
        break;
    case 0x42:
        VoiceAssistant_Send1(0x06, AppState.currentHR);
        break;
    case 0x11:
        // 语音指令：仅连接心率
        EventMsg e11;
        e11.type = EVT_VOICE;
        e11.action = ACT_SENSOR_CMD;
        e11.value = 11;
        Event_Push(e11);
        break;
    case 0x12:
        // 语音指令：仅连接踏频
        EventMsg e12;
        e12.type = EVT_VOICE;
        e12.action = ACT_SENSOR_CMD;
        e12.value = 12;
        Event_Push(e12);
        break;
    case 0x13:
        // 语音指令：仅断开心率
        EventMsg e13;
        e13.type = EVT_VOICE;
        e13.action = ACT_SENSOR_CMD;
        e13.value = 13;
        Event_Push(e13);
        break;
    case 0x14:
        // 语音指令：仅断开踏频
        EventMsg e14;
        e14.type = EVT_VOICE;
        e14.action = ACT_SENSOR_CMD;
        e14.value = 14;
        Event_Push(e14);
        break;
    case 0x43:
        isRiding = true;
        rideStartSysTime = millis();
        Serial.println("[🚴‍♂️] 开始记录骑行");
        break;
    case 0x45:
        rideDistanceKm = 0;
        Serial.println("[🚴‍♂️] 记录清零");
        break;
    case 0x44:
    case 0x57:
    {
        uint32_t sec = isRiding ? ((millis() - rideStartSysTime) / 1000) : 0;
        uint8_t hh = sec / 3600;
        uint8_t mm = (sec % 3600) / 60;
        uint8_t k_i = (uint8_t)rideDistanceKm;
        uint8_t k_d = (uint8_t)((rideDistanceKm - k_i) * 10);
        VoiceAssistant_Send4(0x09, k_i, k_d, hh, mm);
        if (cmd == 0x44)
            isRiding = false;
    }
    break;
    case 0x05:
    {
        int targetBrightness = AppState.brightness;
        if (param == 1)
            targetBrightness = min(255, AppState.brightness + 50);
        else
            targetBrightness = max(5, AppState.brightness - 50);

        EventMsg e = {EVT_VOICE, ACT_SYS_BRIGHTNESS, (uint32_t)targetBrightness};
        Event_Push(e);
        break;
    }
    case 0x03:
    {
        EventMsg e = {EVT_VOICE, ACT_MODE_SWITCH, MODE_OFF};
        Event_Push(e);
        break;
    }
    case 0x04:
    case 0x52:
    {
        EventMsg e = {EVT_VOICE, ACT_MODE_SWITCH, MODE_CLOCK};
        Event_Push(e);
        break;
    }
    case 0x54:
    {
        EventMsg e = {EVT_VOICE, ACT_MODE_SWITCH, MODE_TIMER};
        Event_Push(e);
        break;
    }
    case 0x55:
    {
        EventMsg e = {EVT_VOICE, ACT_MODE_SWITCH, MODE_COUNTDOWN};
        Event_Push(e);
        break;
    }
    case 0x56:
    {
        EventMsg e = {EVT_VOICE, ACT_MODE_SWITCH, MODE_ALARM};
        Event_Push(e);
        break;
    }
    case 0x58:
    {
        EventMsg e = {EVT_VOICE, ACT_MODE_SWITCH, MODE_SENSOR_CSC};
        Event_Push(e);
        break;
    }
    case 0x59:
    {
        EventMsg e = {EVT_VOICE, ACT_MODE_SWITCH, MODE_SENSOR_HRM};
        Event_Push(e);
        break;
    }
    case 0x5A:
    {
        // ✅ 先切换模式
        EventMsg eMode = {EVT_VOICE, ACT_MODE_SWITCH, MODE_ALARM};
        Event_Push(eMode);
        // ✅ 再切换索引
        uint8_t newIdx = (AppState.alarmDisplayIndex + 1) % 3;
        EventMsg eIdx = {EVT_VOICE, ACT_ALARM_SET_INDEX, newIdx};
        Event_Push(eIdx);
        break;
    }
    case 0x5B:
    {
        // ✅ 确保在闹钟模式
        EventMsg eMode = {EVT_VOICE, ACT_MODE_SWITCH, MODE_ALARM};
        Event_Push(eMode);

        // ✅ 参数打包：(idx<<24) | (en<<16) | (h<<8) | m
        uint8_t idx = AppState.alarmDisplayIndex;
        uint32_t packed = ((uint32_t)idx << 24) |
                          ((uint32_t)1 << 16) | // en = true
                          ((uint32_t)AppState.alarms[idx].hour << 8) |
                          ((uint32_t)AppState.alarms[idx].minute);

        EventMsg eSave = {EVT_VOICE, ACT_ALARM_SAVE, packed};
        Event_Push(eSave);
        break;
    }
    case 0x5C:
    {
        // 1. 切到闹钟模式
        EventMsg eMode = {EVT_VOICE, ACT_MODE_SWITCH, MODE_ALARM};
        Event_Push(eMode);

        // 2. 打包参数并保存 (en = false)
        uint8_t idx = AppState.alarmDisplayIndex;
        uint32_t packed = ((uint32_t)idx << 24) |
                          ((uint32_t)0 << 16) |
                          ((uint32_t)AppState.alarms[idx].hour << 8) |
                          ((uint32_t)AppState.alarms[idx].minute);

        EventMsg eSave = {EVT_VOICE, ACT_ALARM_SAVE, packed};
        Event_Push(eSave);
        break;
    }
    case 0x5D:
    {
        // 1. 切到闹钟模式
        EventMsg eMode = {EVT_VOICE, ACT_MODE_SWITCH, MODE_ALARM};
        Event_Push(eMode);

        // 2. 打包参数并清除 (h = 0, m = 0, en = 0)
        uint8_t idx = AppState.alarmDisplayIndex;
        uint32_t packed = ((uint32_t)idx << 24) |
                          ((uint32_t)0 << 16) |
                          ((uint32_t)0 << 8) |
                          ((uint32_t)0);

        EventMsg eSave = {EVT_VOICE, ACT_ALARM_SAVE, packed};
        Event_Push(eSave);
        break;
    }
    case 0x32:
    {
        // ✅ 停止响铃事件
        EventMsg eStop = {EVT_VOICE, ACT_ALARM_STOP_RING, 0};
        Event_Push(eStop);
        // ✅ 停止倒计时（如果需要）
        EventMsg eCdReset = {EVT_VOICE, ACT_CDOWN_RESET, 0};
        Event_Push(eCdReset);
        // ✅ 切回时钟
        EventMsg eMode = {EVT_VOICE, ACT_MODE_SWITCH, MODE_CLOCK};
        Event_Push(eMode);
        break;
    }
    case 0x33:
        for (int i = 0; i < 3; i++)
        {
            if (AppState.alarms[i].isSet)
            {
                AppState.saveAlarm(i, false, AppState.alarms[i].hour, AppState.alarms[i].minute);
                WebGateway_BroadcastAlarmState(i);
            }
        }
        break;
    case 0x40:
        TimeSync_Init();
        break;
    case 0x51:
        VoiceAssistant_Beep(50);
        delay(100);
        VoiceAssistant_Beep(100);
        break;
    }
}

void VoiceAssistant_Init()
{
    VoiceSerial.begin(115200, SERIAL_8N1, PIN_VOICE_RX, PIN_VOICE_TX);
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);

    VoiceInputMachine::init(); // 【新增】初始化状态机

    Serial.println("🎙️ 语音副脑 V29 已挂载 (搭载 FSM 状态引擎).");
}

void VoiceAssistant_Loop()
{
    static uint8_t rxState = 0, rxCmd = 0, rxParam = 0;
    while (VoiceSerial.available())
    {
        uint8_t b = VoiceSerial.read();
        if (rxState == 0 && b == 0xAA)
            rxState = 1;
        else if (rxState == 1)
        {
            if (b == 0x55)
                rxState = 2;
            else
                rxState = 0;
        }
        else if (rxState == 2)
        {
            rxCmd = b;
            rxState = 3;
        }
        else if (rxState == 3)
        {
            rxParam = b;
            rxState = 4;
        }
        else if (rxState == 4)
        {
            if (b == 0x55)
                rxState = 5;
            else
                rxState = 0;
        }
        else if (rxState == 5)
        {
            if (b == 0xAA)
                ProcessVoiceCommand(rxCmd, rxParam);
            rxState = 0;
        }
        else
            rxState = 0;
    }
    VoiceInputMachine::loop();

    static unsigned long lastCalc = 0;
    if (isRiding && millis() - lastCalc >= 1000)
    {
        lastCalc = millis();
        if (AppState.currentCadence > 0)
            rideDistanceKm += (AppState.currentCadence / 60.0f) * 5.0f / 1000.0f;
    }

    if (AppState.currentMode == MODE_COUNTDOWN && AppState.isCountdownRunning)
    {
        uint32_t rem = AppState.countdownRemaining - ((millis() - AppState.countdownStartSysTime) / 1000);
        if (rem <= 10 && rem > 0 && rem != lastReportedSec)
        {
            lastReportedSec = rem;
            VoiceAssistant_Send1(0x04, rem);
        }
    }
    else
        lastReportedSec = 99;

    static unsigned long lastTick = 0;
    static bool buzzerState = false;
    if (metronomeEnabled && metronomeBpm > 0)
    {
        unsigned long interval = 60000 / metronomeBpm;
        if (!buzzerState && (millis() - lastTick >= interval))
        {
            lastTick = millis();
            digitalWrite(PIN_BUZZER, HIGH);
            buzzerState = true;
        }
        if (buzzerState && (millis() - lastTick >= 30))
        {
            digitalWrite(PIN_BUZZER, LOW);
            buzzerState = false;
        }
    }
    else
    {
        if (buzzerState)
        {
            digitalWrite(PIN_BUZZER, LOW);
            buzzerState = false;
        }
    }

    static unsigned long lastZoneCheck = 0;
    if (hrZone > 0 && millis() - lastZoneCheck >= 10000)
    {
        lastZoneCheck = millis();
        uint8_t hr = AppState.currentHR;
        if (hr > 0)
        {
            if (hrZone == 1)
            {
                if (hr < 110)
                    VoiceAssistant_Send0(0x07);
                else if (hr > 130)
                    VoiceAssistant_Send0(0x08);
            }
            else if (hrZone == 2)
            {
                if (hr < 130)
                    VoiceAssistant_Send0(0x07);
                else if (hr > 150)
                    VoiceAssistant_Send0(0x08);
            }
        }
    }
}