#include "WebGateway.h"
#include "GlobalState.h"
#include "DisplayCore.h"
#include <NimBLEDevice.h>
#include <sys/time.h>
#include <vector>

NimBLECharacteristic *pTxCharacteristic = nullptr;

class RxCallbacks : public NimBLECharacteristicCallbacks
{
    void onWrite(NimBLECharacteristic *pChar)
    {
        std::string rx = pChar->getValue();
        if (rx.length() == 0)
            return;
        uint8_t cmd = (uint8_t)rx[0];

        Serial.printf("\n[🌐 WEB->ESP] 收到报文 CMD: 0x%02X, 长度: %d bytes\n", cmd, rx.length());

        if (cmd == 0x01 && rx.length() >= 2)
        {
            Display_SetBrightness(rx[1]);
            Display_Show();
            AppState.saveBrightness(rx[1]);
            Serial.printf("[🔆 UI] 亮度配置已下发: %d\n", rx[1]);
        }
        else if (cmd == 0x02 && rx.length() >= 4)
        {
            Display_Fill(CRGB(rx[1], rx[2], rx[3]));
            Display_Show();
            AppState.currentMode = MODE_OFF;
        }
        else if (cmd == 0x03)
        {
            Display_Clear();
            Display_Show();
            AppState.currentMode = MODE_OFF;
            Serial.println("[📺 UI] 执行强制息屏");
        }
        else if (cmd == 0x04 && rx.length() >= 5)
        {
            uint32_t ts = ((uint32_t)rx[1] << 24) | ((uint32_t)rx[2] << 16) | ((uint32_t)rx[3] << 8) | (uint32_t)rx[4];
            struct timeval tv = {.tv_sec = (time_t)ts, .tv_usec = 0};
            settimeofday(&tv, NULL);
            AppState.currentMode = MODE_CLOCK;
            Display_Clear();
            Display_Show();
            Serial.println("[🕒 UI] 时间同步，切入时钟模式");
        }
        else if (cmd == 0x05)
        {
            AppState.pendingCmd = 5;
            Serial.println("[⚡ TASK] 雷达扫描入队");
        }
        else if (cmd == 0x06 && rx.length() > 2)
        {
            AppState.pendingAddr = NimBLEAddress(rx.substr(2), rx[1]);
            AppState.pendingCmd = 6;
            Serial.printf("[⚡ TASK] 连接指令入队 -> %s\n", rx.substr(2).c_str());
        }
        else if (cmd == 0x07 && rx.length() > 2)
        {
            AppState.pendingAddr = NimBLEAddress(rx.substr(2), rx[1]);
            AppState.pendingCmd = 7;
            Serial.printf("[⚡ TASK] 断开指令入队 -> %s\n", rx.substr(2).c_str());
        }
        else if (cmd == 0x08)
        {
            AppState.currentMode = MODE_SENSOR;
            Display_Clear();
            Display_Show();
            Serial.println("[🚴 UI] 运动数据面板");
        }
        else if (cmd == 0x09)
        {
            AppState.currentMode = MODE_TIMER;
            Display_Clear();
            Display_Show();
            Serial.println("[⏱️ UI] 秒表面板");
        }
        else if (cmd == 0x0A && rx.length() >= 2)
        {
            uint8_t action = rx[1];
            if (action == 0x01 && !AppState.isTimerRunning)
            {
                AppState.timerStartTime = millis();
                AppState.isTimerRunning = true;
                AppState.currentMode = MODE_TIMER;
            }
            else if (action == 0x00 && AppState.isTimerRunning)
            {
                AppState.timerElapsed += millis() - AppState.timerStartTime;
                AppState.isTimerRunning = false;
            }
            else if (action == 0x02)
            {
                AppState.timerElapsed = 0;
                AppState.isTimerRunning = false;
            }
        }
        else if (cmd == 0x0B)
        {
            AppState.currentMode = MODE_COUNTDOWN;
            Display_Clear();
            Display_Show();
            Serial.println("[⏳ UI] 倒计表面板");
        }
        else if (cmd == 0x0C && rx.length() >= 2)
        {
            AppState.countdownTotalSeconds = rx[1] * 60;
            AppState.countdownRemaining = AppState.countdownTotalSeconds;
            AppState.isCountdownRunning = AppState.isCountdownFinished = false;
            AppState.currentMode = MODE_COUNTDOWN;
            Serial.printf("[⏳ CDOWN] 倒计时预设更新为: %d 分钟\n", rx[1]);
        }
        else if (cmd == 0x0D && rx.length() >= 2)
        {
            uint8_t action = rx[1];
            if (action == 0x01 && !AppState.isCountdownRunning && AppState.countdownRemaining > 0)
            {
                AppState.countdownStartSysTime = millis();
                AppState.isCountdownRunning = true;
                AppState.isCountdownFinished = false;
                AppState.currentMode = MODE_COUNTDOWN;
            }
            else if (action == 0x00 && AppState.isCountdownRunning)
            {
                uint32_t elapsed = (millis() - AppState.countdownStartSysTime) / 1000;
                AppState.countdownRemaining = (elapsed < AppState.countdownRemaining) ? (AppState.countdownRemaining - elapsed) : 0;
                AppState.isCountdownRunning = false;
            }
            else if (action == 0x02)
            {
                AppState.countdownRemaining = AppState.countdownTotalSeconds;
                AppState.isCountdownRunning = AppState.isCountdownFinished = false;
            }
        }
        else if (cmd == 0x0E)
        {
            if (AppState.currentMode == MODE_ALARM)
                AppState.alarmDisplayIndex = (AppState.alarmDisplayIndex + 1) % 3;
            else
            {
                AppState.currentMode = MODE_ALARM;
                AppState.alarmDisplayIndex = 0;
            }
            Display_Clear();
            Display_Show();
            Serial.printf("[⏰ UI] 切至硬件闹钟检视, 组别: %d\n", AppState.alarmDisplayIndex + 1);
        }
        else if (cmd == 0x0F && rx.length() >= 5)
        {
            uint8_t idx = rx[1];
            if (idx < 3)
            {
                AppState.saveAlarm(idx, rx[2], rx[3], rx[4]);
                AppState.alarms[idx].isRinging = false;
            }
        }
        else if (cmd == 0x10)
        {
            Serial.println("[🤝 SYNC] Web建立连接，推送系统状态快照...");
            vector<uint8_t> p1 = {0x10, (uint8_t)AppState.currentMode, AppState.brightness, (uint8_t)(AppState.autoReconnect ? 1 : 0)};
            pTxCharacteristic->setValue(p1.data(), p1.size());
            pTxCharacteristic->notify();
            delay(50);
            for (int i = 0; i < 3; i++)
            {
                AlarmData &a = AppState.alarms[i];
                // 解决窄化转换警告
                vector<uint8_t> p2 = {0x11, (uint8_t)i, (uint8_t)(a.isSet ? 1 : 0), (uint8_t)(a.enabled ? 1 : 0), a.hour, a.minute};
                pTxCharacteristic->setValue(p2.data(), p2.size());
                pTxCharacteristic->notify();
                delay(50);
            }
            if (AppState.savedHrmMac != "")
                WebGateway_SendScanResult(1, 0, AppState.savedHrmMac, "NVS_已存心率");
            if (AppState.savedCscMac != "")
                WebGateway_SendScanResult(2, 0, AppState.savedCscMac, "NVS_已存踏频");
        }
        else if (cmd == 0x13 && rx.length() >= 2)
        {
            AppState.saveAutoReconnect(rx[1] == 1);
        }
        else if (cmd == 0x14 && rx.length() > 2)
        {
            AppState.saveSensorMac(rx[1], rx.substr(2));
        }
    }
};

void WebGateway_Init()
{
    NimBLEServer *pServer = NimBLEDevice::createServer();
    NimBLEService *pService = pServer->createService(SERVICE_UUID);
    auto pRx = pService->createCharacteristic(CHAR_RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    pRx->setCallbacks(new RxCallbacks());
    pTxCharacteristic = pService->createCharacteristic(CHAR_TX_UUID, NIMBLE_PROPERTY::NOTIFY);
    pService->start();
    NimBLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID);
    NimBLEDevice::getAdvertising()->start();
}

void WebGateway_SendScanResult(uint8_t type, uint8_t addrType, std::string macStr, std::string name)
{
    std::vector<uint8_t> payload = {0x05, type, addrType, (uint8_t)macStr.length()};
    for (char c : macStr)
        payload.push_back(c);
    for (char c : name)
        payload.push_back(c);
    if (pTxCharacteristic)
    {
        pTxCharacteristic->setValue(payload.data(), payload.size());
        pTxCharacteristic->notify();
    }
}