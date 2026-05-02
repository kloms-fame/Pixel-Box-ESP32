#include "VoiceAssistant.h"
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
        AppState.countdownTotalSeconds = param * 60;
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
        AppState.isCountdownRunning = false;
        AppState.currentMode = MODE_COUNTDOWN;
        Display_Clear();
        Display_Show();
        VoiceAssistant_Send1(0x01, param);
        break;
    case 0x0D:
        if (param == 1)
        {
            AppState.countdownStartSysTime = millis();
            AppState.isCountdownRunning = true;
            AppState.isCountdownFinished = false;
        }
        else if (param == 0)
        {
            AppState.isCountdownRunning = false;
        }
        else
        {
            AppState.countdownRemaining = AppState.countdownTotalSeconds;
            AppState.isCountdownRunning = false;
            AppState.isCountdownFinished = false;
        }
        AppState.currentMode = MODE_COUNTDOWN;
        Display_Clear();
        Display_Show();
        break;
    case 0x0A:
        if (param == 1)
        {
            AppState.timerStartTime = millis();
            AppState.isTimerRunning = true;
        }
        else if (param == 0)
        {
            AppState.timerElapsed += millis() - AppState.timerStartTime;
            AppState.isTimerRunning = false;
        }
        else
        {
            AppState.timerElapsed = 0;
            AppState.isTimerRunning = false;
        }
        AppState.currentMode = MODE_TIMER;
        Display_Clear();
        Display_Show();
        break;
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
        AppState.pendingCmd = 11;
        break;
    case 0x12:
        AppState.pendingCmd = 12;
        break;
    case 0x13:
        AppState.pendingCmd = 13;
        break;
    case 0x14:
        AppState.pendingCmd = 14;
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
        if (param == 1)
        {
            int b = min(255, AppState.brightness + 50);
            AppState.saveBrightness(b);
            Display_SetBrightness(b);
        }
        else
        {
            int b = max(5, AppState.brightness - 50);
            AppState.saveBrightness(b);
            Display_SetBrightness(b);
        }
        Display_Show();
        break;
    case 0x03:
        AppState.currentMode = MODE_OFF;
        Display_Clear();
        Display_Show();
        break;
    case 0x04:
    case 0x52:
        AppState.currentMode = MODE_CLOCK;
        Display_Clear();
        Display_Show();
        break;
    case 0x54:
        AppState.currentMode = MODE_TIMER;
        Display_Clear();
        Display_Show();
        break;
    case 0x55:
        AppState.currentMode = MODE_COUNTDOWN;
        Display_Clear();
        Display_Show();
        break;
    case 0x56:
        AppState.currentMode = MODE_ALARM;
        Display_Clear();
        Display_Show();
        break;
    case 0x58:
        AppState.currentMode = MODE_SENSOR_CSC;
        Display_Clear();
        Display_Show();
        break;
    case 0x59:
        AppState.currentMode = MODE_SENSOR_HRM;
        Display_Clear();
        Display_Show();
        break;
    case 0x5A:
        AppState.currentMode = MODE_ALARM;
        AppState.alarmDisplayIndex = (AppState.alarmDisplayIndex + 1) % 3;
        Display_Clear();
        Display_Show();
        break;
    case 0x5B:
        AppState.currentMode = MODE_ALARM;
        {
            uint8_t idx = AppState.alarmDisplayIndex;
            AppState.saveAlarm(idx, true, AppState.alarms[idx].hour, AppState.alarms[idx].minute);
            WebGateway_BroadcastAlarmState(idx);
            Display_Clear();
            Display_Show();
        }
        break;
    case 0x5C:
        AppState.currentMode = MODE_ALARM;
        {
            uint8_t idx = AppState.alarmDisplayIndex;
            AppState.saveAlarm(idx, false, AppState.alarms[idx].hour, AppState.alarms[idx].minute);
            WebGateway_BroadcastAlarmState(idx);
            Display_Clear();
            Display_Show();
        }
        break;
    case 0x5D:
        AppState.currentMode = MODE_ALARM;
        {
            uint8_t idx = AppState.alarmDisplayIndex;
            AppState.saveAlarm(idx, false, 0, 0);
            AppState.alarms[idx].isSet = false;
            WebGateway_BroadcastAlarmState(idx);
            Display_Clear();
            Display_Show();
        }
        break;
    case 0x32:
        for (int i = 0; i < 3; i++)
            AppState.alarms[i].isRinging = false;
        AppState.isCountdownFinished = false;
        AppState.currentMode = MODE_CLOCK;
        Display_Clear();
        Display_Show();
        break;
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