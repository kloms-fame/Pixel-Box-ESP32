#include "VoiceAssistant.h"
#include "GlobalState.h"
#include "SensorHub.h"
#include "WebGateway.h"
#include "TimeSync.h"
#include "DisplayCore.h"

// 硬件引脚配置 (继承 V25 的极度稳定方案)
#define PIN_VOICE_RX 2 // 对应 SU-03T 的 TX
#define PIN_VOICE_TX 3 // 对应 SU-03T 的 RX
#define PIN_BUZZER 1   // 蜂鸣器引脚

// === 后台 Daemon 状态变量 ===
static bool metronomeEnabled = false;
static uint16_t metronomeBpm = 0;
static uint8_t hrZone = 0;
static unsigned long lastAutoReportTime = 0;
static uint32_t lastReportedSec = 0;

// === 虚拟骑行引擎状态变量 ===
static bool isRiding = false;
static unsigned long rideStartSysTime = 0;
static float rideDistanceKm = 0.0; // 虚拟里程累加器

// ==========================================
// 核心通信组件 1：双变量发送器 (13字节协议)
// ==========================================
void VoiceAssistant_SendVoice(uint8_t msgId, uint32_t val1, uint32_t val2)
{
    uint8_t buf[13] = {0xAA, 0x55, msgId, 0, 0, 0, 0, 0, 0, 0, 0, 0x55, 0xAA};
    buf[3] = (val1 >> 24) & 0xFF;
    buf[4] = (val1 >> 16) & 0xFF;
    buf[5] = (val1 >> 8) & 0xFF;
    buf[6] = val1 & 0xFF;
    buf[7] = (val2 >> 24) & 0xFF;
    buf[8] = (val2 >> 16) & 0xFF;
    buf[9] = (val2 >> 8) & 0xFF;
    buf[10] = val2 & 0xFF;
    Serial1.write(buf, 13);
}

// ==========================================
// 核心通信组件 2：四变量发送器 (专属骑行总结)
// 完美对应你的 RX_0x09: 本次骑行 CS_7 点 CS_8 公里, 用时 CS_9 小时 CS_10 分钟
// ==========================================
void VoiceAssistant_SendRideSummary(uint8_t km_int, uint8_t km_dec, uint8_t hours, uint8_t mins)
{
    uint8_t buf[13] = {0xAA, 0x55, 0x09, 0, 0, 0, 0, 0, 0, 0, 0, 0x55, 0xAA};
    // 巧妙利用 13 字节协议的中间 4 个字节，装载 4 个小变量
    buf[3] = km_int;
    buf[4] = km_dec;
    buf[5] = hours;
    buf[6] = mins;
    Serial1.write(buf, 13);
    Serial.printf("[🎙️ 总结] 骑行总结已下发: %d.%d KM, %d H %d M\n", km_int, km_dec, hours, mins);
}

// ==========================================
// 极客级大脑：51 项全功能路由解析器
// ==========================================
void ProcessVoiceCommand(uint8_t cmd, uint8_t param)
{
    Serial.printf("\n[🎙️ VOICE] 👉 执行硬核指令: CMD[0x%02X] PARAM[%d]\n", cmd, param);

    switch (cmd)
    {
    // ------------------------------------
    // 1. 倒计时引擎 (0x0C, 0x0D)
    // ------------------------------------
    case 0x0C: // 设倒计时
        AppState.countdownTotalSeconds = param * 60;
        AppState.countdownRemaining = AppState.countdownTotalSeconds;
        AppState.isCountdownRunning = false;
        AppState.currentMode = MODE_COUNTDOWN;
        WebGateway_BroadcastCdownConfig();
        VoiceAssistant_SendVoice(0x01, param, 0); // RX_0x01: 收到，已设XX分
        break;
    case 0x0D: // 倒计时控制
        if (param == 1 && AppState.countdownRemaining > 0)
        {
            AppState.countdownStartSysTime = millis();
            AppState.isCountdownRunning = true;
            AppState.isCountdownFinished = false;
        }
        else if (param == 0)
        {
            uint32_t elapsed = (millis() - AppState.countdownStartSysTime) / 1000;
            AppState.countdownRemaining = (elapsed < AppState.countdownRemaining) ? (AppState.countdownRemaining - elapsed) : 0;
            AppState.isCountdownRunning = false;
        }
        else if (param == 2)
        {
            AppState.countdownRemaining = AppState.countdownTotalSeconds;
            AppState.isCountdownRunning = false;
            AppState.isCountdownFinished = false;
        }
        AppState.currentMode = MODE_COUNTDOWN;
        break;

    // ------------------------------------
    // 2. 正向秒表 (0x0A)
    // ------------------------------------
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
        else if (param == 2)
        {
            AppState.timerElapsed = 0;
            AppState.isTimerRunning = false;
        }
        AppState.currentMode = MODE_TIMER;
        break;

    // ------------------------------------
    // 3. 骑行辅助与环境感知 (0x34-0x3A, 0x41-0x42)
    // ------------------------------------
    case 0x34:
        metronomeEnabled = true;
        metronomeBpm = param;
        break;
    case 0x35:
        metronomeEnabled = false;
        break;
    case 0x36:
        hrZone = 1;
        break; // 燃脂
    case 0x37:
        hrZone = 2;
        break; // 有氧
    case 0x38:
        hrZone = 0;
        break; // 退出区间
    case 0x39:
        VoiceAssistant_SendVoice(0x03, AppState.currentHR, AppState.currentCadence);
        break; // 报双项
    case 0x3A:
    {
        time_t now;
        struct tm ti;
        time(&now);
        localtime_r(&now, &ti);
        VoiceAssistant_SendVoice(0x02, ti.tm_hour, ti.tm_min);
    }
    break;
    case 0x41:
        VoiceAssistant_SendVoice(0x05, AppState.currentCadence, 0);
        break; // 单报踏频
    case 0x42:
        VoiceAssistant_SendVoice(0x06, AppState.currentHR, 0);
        break; // 单报心率

    // ------------------------------------
    // 4. 骑行记录引擎 (0x43-0x45)
    // ------------------------------------
    case 0x43:
        isRiding = true;
        rideStartSysTime = millis();
        Serial.println("[🚴‍♂️ RECORD] 骑行轨迹开始记录");
        break;
    case 0x44:
        if (isRiding)
        {
            isRiding = false;
            uint32_t totalSec = (millis() - rideStartSysTime) / 1000;
            uint8_t hours = totalSec / 3600;
            uint8_t mins = (totalSec % 3600) / 60;
            uint8_t km_int = (uint8_t)rideDistanceKm;
            uint8_t km_dec = (uint8_t)((rideDistanceKm - km_int) * 10);
            VoiceAssistant_SendRideSummary(km_int, km_dec, hours, mins); // 触发终极总结播报
        }
        break;
    case 0x45:
        rideDistanceKm = 0.0;
        rideStartSysTime = millis();
        Serial.println("[🚴‍♂️ RECORD] 骑行记录已清零");
        break;

    // ------------------------------------
    // 5. 蓝牙与底层设备管理 (0x11-0x14)
    // ------------------------------------
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

    // ------------------------------------
    // 6. 系统界面与打断机制 (0x03-0x05, 0x32, 0x40, 0x99)
    // ------------------------------------
    case 0x32:
        for (int i = 0; i < 3; i++)
            AppState.alarms[i].isRinging = false; // 强行斩断所有闹钟响铃
        AppState.isCountdownFinished = false;
        AppState.currentMode = AppState.previousMode;
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
        break; // 瞬态校时
    case 0x03:
        AppState.currentMode = MODE_OFF;
        break; // 息屏
    case 0x04:
        AppState.currentMode = MODE_CLOCK;
        break; // 亮屏
    case 0x05: // 语音控光
        if (param == 1)
        {
            int b = AppState.brightness + 30;
            if (b > 255)
                b = 255;
            AppState.saveBrightness(b);
            Display_SetBrightness(b);
            Display_Show();
        }
        else
        {
            int b = AppState.brightness - 30;
            if (b < 5)
                b = 5;
            AppState.saveBrightness(b);
            Display_SetBrightness(b);
            Display_Show();
        }
        break;
    case 0x99:
        AppState.pendingCmd = 99;
        break; // 召唤出厂重置/重启

    // ------------------------------------
    // 7. 彩蛋交互 (0x51) - 对应表格里的“好累啊”
    // ------------------------------------
    case 0x51:
        // 给你两声清脆的蜂鸣器滴答，假装在鼓励你
        digitalWrite(PIN_BUZZER, HIGH);
        delay(50);
        digitalWrite(PIN_BUZZER, LOW);
        delay(100);
        digitalWrite(PIN_BUZZER, HIGH);
        delay(100);
        digitalWrite(PIN_BUZZER, LOW);
        Serial.println("[💡 EASTER EGG] 触发疲劳安抚彩蛋");
        break;
    }
}

// ==========================================
// 初始化挂载
// ==========================================
void VoiceAssistant_Init()
{
    Serial1.begin(115200, SERIAL_8N1, PIN_VOICE_RX, PIN_VOICE_TX);
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    Serial.println("🎙️ 离线语音 V26 终极形态已挂载 (Baud=115200, 51项指令全开)");
}

// ==========================================
// 无阻塞守护进程 (Daemon)
// ==========================================
void VoiceAssistant_Loop()
{
    static uint8_t rxState = 0;
    static uint8_t rxCmd = 0;
    static uint8_t rxParam = 0;

    // 1. 滑动窗口抓包器 (强行从噪音中提取有效帧)
    while (Serial1.available())
    {
        uint8_t b = Serial1.read();
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

    // 2. 虚拟骑行里程累加器 (踏频转里程)
    // 假设：自行车轮长2.1米，齿比假设为 2.5 (踩一圈轮子转2.5圈) -> 踩一圈约前进 5米
    static unsigned long lastCalcTime = 0;
    if (isRiding && millis() - lastCalcTime >= 1000)
    {
        lastCalcTime = millis();
        if (AppState.currentCadence > 0)
        {
            float speed_km_s = (AppState.currentCadence / 60.0) * 5.0 / 1000.0;
            rideDistanceKm += speed_km_s;
        }
    }

    // 3. 节拍器守护进程
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

    // 4. 倒计时最后 10 秒死磕播报
    if (AppState.currentMode == MODE_COUNTDOWN && AppState.isCountdownRunning)
    {
        uint32_t elapsed = (millis() - AppState.countdownStartSysTime) / 1000;
        uint32_t rem = (AppState.countdownTotalSeconds > elapsed) ? AppState.countdownTotalSeconds - elapsed : 0;
        if (rem <= 10 && rem > 0 && rem != lastReportedSec)
        {
            lastReportedSec = rem;
            VoiceAssistant_SendVoice(0x04, rem, 0); // RX_0x04
        }
    }
    else
        lastReportedSec = 0;

    // 5. 心率区间智能预警 (每10秒扫描一次)
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
                    VoiceAssistant_SendVoice(0x07, 0, 0); // RX_0x07: 心率下降
                else if (hr > 130)
                    VoiceAssistant_SendVoice(0x08, 0, 0); // RX_0x08: 心率过高
            }
            else if (hrZone == 2)
            {
                if (hr < 130)
                    VoiceAssistant_SendVoice(0x07, 0, 0);
                else if (hr > 150)
                    VoiceAssistant_SendVoice(0x08, 0, 0);
            }
        }
    }
}