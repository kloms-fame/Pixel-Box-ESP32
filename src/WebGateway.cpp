#include "WebGateway.h"
#include "EventBus.h"
#include "GlobalState.h"
#include "DisplayCore.h"
#include "SensorHub.h"
#include <NimBLEDevice.h>
#include <sys/time.h>
#include <vector>

NimBLECharacteristic *pTxCharacteristic = nullptr;

class WebServerCallbacks : public NimBLEServerCallbacks
{
    void onConnect(NimBLEServer *pServer) override
    {
        Serial.println("[🌐 WEB] 控制端已接入");
    }
    void onDisconnect(NimBLEServer *pServer) override
    {
        Serial.println("[🌐 WEB] 控制端已断开，正在重新启动蓝牙广播...");
        NimBLEDevice::startAdvertising();
    }
};

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
            uint8_t b = rx[1];

            // ✅ Phase B：只发事件，不直接操作
            EventMsg e;
            e.type = EVT_WEB;
            e.action = ACT_SYS_BRIGHTNESS;
            e.value = b;
            Event_Push(e);
        }
        else if (cmd == 0x02 && rx.length() >= 4)
        {
            Display_Fill(CRGB(rx[1], rx[2], rx[3]));

            AppState.currentMode = MODE_OFF;
        }
        else if (cmd == 0x03)
        {
            EventMsg e;
            e.type = EVT_WEB;
            e.action = ACT_MODE_SWITCH;
            e.value = MODE_OFF;
            Event_Push(e);
            Serial.println("[📺 UI] 执行强制息屏");
        }
        else if (cmd == 0x04 && rx.length() >= 5)
        {
            // ✅ Phase B：时间同步比较特殊，涉及系统时间设置，这部分保留原样
            // 但切回时钟模式改为事件
            uint32_t ts = ((uint32_t)rx[1] << 24) | ((uint32_t)rx[2] << 16) | ((uint32_t)rx[3] << 8) | (uint32_t)rx[4];
            struct timeval tv = {.tv_sec = (time_t)ts, .tv_usec = 0};
            settimeofday(&tv, NULL);

            // ✅ 切模式改为事件
            EventMsg e;
            e.type = EVT_WEB;
            e.action = ACT_MODE_SWITCH;
            e.value = MODE_CLOCK;
            Event_Push(e);

            Serial.println("[🕒 UI] 时间同步，切入时钟模式");
        }
        else if (cmd == 0x05)
        {
            // ✅ Phase B：改为事件驱动 (使用 ACT_SENSOR_CMD)
            EventMsg e;
            e.type = EVT_WEB;
            e.action = ACT_SENSOR_CMD;
            e.value = 5; // 对应原 pendingCmd = 5
            Event_Push(e);

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
            // ✅ Phase B：改为事件驱动
            AppMode targetMode = MODE_SENSOR_HRM;
            if (AppState.currentMode == MODE_SENSOR_HRM)
                targetMode = MODE_SENSOR_CSC;
            else if (AppState.currentMode == MODE_SENSOR_CSC)
                targetMode = MODE_SENSOR_HRM;
            else
            {
                if (SensorHub_HasCSC() && !SensorHub_HasHRM())
                    targetMode = MODE_SENSOR_CSC;
            }

            EventMsg e;
            e.type = EVT_WEB;
            e.action = ACT_MODE_SWITCH;
            e.value = targetMode;
            Event_Push(e);

            Serial.println("[🚴 UI] 切换并进入运动数据面板");
        }
        else if (cmd == 0x09)
        {
            // ✅ Phase B：改为事件驱动
            EventMsg e;
            e.type = EVT_WEB;
            e.action = ACT_MODE_SWITCH;
            e.value = MODE_TIMER;
            Event_Push(e);
        }
        else if (cmd == 0x0A && rx.length() >= 2)
        {
            // ✅ Phase B：改为事件驱动
            uint8_t action = rx[1];

            // 1. 先确保在秒表模式
            EventMsg eMode;
            eMode.type = EVT_WEB;
            eMode.action = ACT_MODE_SWITCH;
            eMode.value = MODE_TIMER;
            Event_Push(eMode);

            // 2. 再发送控制指令
            if (action == 0x01)
            {
                EventMsg e;
                e.type = EVT_WEB;
                e.action = ACT_TIMER_START;
                e.value = 0;
                Event_Push(e);
            }
            else if (action == 0x00)
            {
                EventMsg e;
                e.type = EVT_WEB;
                e.action = ACT_TIMER_PAUSE;
                e.value = 0;
                Event_Push(e);
            }
            else if (action == 0x02)
            {
                EventMsg e;
                e.type = EVT_WEB;
                e.action = ACT_TIMER_RESET;
                e.value = 0;
                Event_Push(e);
            }
        }
        else if (cmd == 0x0B)
        {
            // ✅ Phase B：改为事件驱动
            EventMsg e;
            e.type = EVT_WEB;
            e.action = ACT_MODE_SWITCH;
            e.value = MODE_COUNTDOWN;
            Event_Push(e);
        }
        else if (cmd == 0x0C && rx.length() >= 2)
        {
            // ✅ Phase B：改为事件驱动
            EventMsg eSet;
            eSet.type = EVT_WEB;
            eSet.action = ACT_CDOWN_SET;
            eSet.value = (uint32_t)rx[1] * 60;
            Event_Push(eSet);

            EventMsg eMode;
            eMode.type = EVT_WEB;
            eMode.action = ACT_MODE_SWITCH;
            eMode.value = MODE_COUNTDOWN;
            Event_Push(eMode);
        }
        else if (cmd == 0x0D && rx.length() >= 2)
        {
            // ✅ Phase B：改为事件驱动
            uint8_t action = rx[1];

            // 1. 先确保在倒计时模式
            EventMsg eMode;
            eMode.type = EVT_WEB;
            eMode.action = ACT_MODE_SWITCH;
            eMode.value = MODE_COUNTDOWN;
            Event_Push(eMode);

            // 2. 再发送控制指令
            if (action == 0x01)
            {
                EventMsg e;
                e.type = EVT_WEB;
                e.action = ACT_CDOWN_START;
                e.value = 0;
                Event_Push(e);
            }
            else if (action == 0x00)
            {
                EventMsg e;
                e.type = EVT_WEB;
                e.action = ACT_CDOWN_PAUSE;
                e.value = 0;
                Event_Push(e);
            }
            else if (action == 0x02)
            {
                EventMsg e;
                e.type = EVT_WEB;
                e.action = ACT_CDOWN_RESET;
                e.value = 0;
                Event_Push(e);
            }
        }
        else if (cmd == 0x0E)
        {
            // ✅ Phase B：改为事件驱动
            // 1. 先切到闹钟模式
            EventMsg eMode;
            eMode.type = EVT_WEB;
            eMode.action = ACT_MODE_SWITCH;
            eMode.value = MODE_ALARM;
            Event_Push(eMode);

            // 2. 计算新索引并发送事件
            // 注意：因为事件是异步的，我们这里基于当前值计算下一个索引
            uint8_t nextIdx = (AppState.alarmDisplayIndex + 1) % 3;

            EventMsg eIdx;
            eIdx.type = EVT_WEB;
            eIdx.action = ACT_ALARM_SET_INDEX;
            eIdx.value = nextIdx;
            Event_Push(eIdx);
        }
        else if (cmd == 0x0F && rx.length() >= 5)
        {
            // ✅ Phase B：改为事件驱动 (参数打包)
            uint8_t idx = rx[1];
            uint8_t en = rx[2];
            uint8_t h = rx[3];
            uint8_t m = rx[4];

            if (idx < 3)
            {
                uint32_t packed = ((uint32_t)idx << 24) |
                                  ((uint32_t)en << 16) |
                                  ((uint32_t)h << 8) |
                                  ((uint32_t)m);

                EventMsg e;
                e.type = EVT_WEB;
                e.action = ACT_ALARM_SAVE;
                e.value = packed;
                Event_Push(e);
            }
        }
        else if (cmd == 0x10)
        {
            Serial.println("[🤝 SYNC] Web请求快照，下发系统状态及设备记忆...");
            WebGateway_BroadcastBasicState();
            delay(50);
            for (int i = 0; i < 3; i++)
            {
                WebGateway_BroadcastAlarmState(i);
                delay(50);
            }
            WebGateway_BroadcastSavedDevices();

            delay(50);
            std::vector<uint8_t> pW = {0x17, (uint8_t)AppState.wifiSSID.length()};
            for (char c : AppState.wifiSSID)
                pW.push_back(c);
            if (pTxCharacteristic)
            {
                pTxCharacteristic->setValue(pW.data(), pW.size());
                pTxCharacteristic->notify();
            }
        }
        else if (cmd == 0x13 && rx.length() >= 2)
        {
            // ✅ Phase B：改为事件驱动
            EventMsg e;
            e.type = EVT_WEB;
            e.action = ACT_SYS_AUTOREC;
            e.value = (uint32_t)(rx[1] == 1 ? 1 : 0);
            Event_Push(e);
        }
        else if (cmd == 0x14 && rx.length() > 2)
        {
            // 🌟 修复：收到 NVS 写入指令后保存，并反向通知 UI 列表刷新！
            AppState.saveSensorMac(rx[1], rx.substr(2));
            WebGateway_BroadcastSavedDevices();
        }
        else if (cmd == 0x15 && rx.length() >= 2)
        {
            // 🌟 修复：处理前端发来的 "忘记设备" 请求，清除后通知 UI 刷新！
            AppState.clearSensorMac(rx[1]);
            WebGateway_BroadcastSavedDevices();
        }
        else if (cmd == 0x20 && rx.length() >= 2)
        {
            size_t pos = 1;
            uint8_t ssidLen = rx[pos++];
            if (rx.length() >= pos + ssidLen)
            {
                std::string ssid = rx.substr(pos, ssidLen);
                pos += ssidLen;
                std::string pass = "";
                if (rx.length() > pos)
                {
                    uint8_t passLen = rx[pos++];
                    if (rx.length() >= pos + passLen)
                    {
                        pass = rx.substr(pos, passLen);
                    }
                }
                AppState.saveWiFi(ssid, pass);
                Serial.println("[🌐 WEB] 收到并保存新 WiFi 配置");
            }
        }
        else if (cmd == 0xFF)
        {
            Serial.println("[🌐 WEB->ESP] 收到恢复出厂设置指令，加入主循环安全执行队列...");
            AppState.pendingCmd = 99;
        }
    }
};

void WebGateway_Init()
{
    NimBLEServer *pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new WebServerCallbacks());
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

void WebGateway_BroadcastBasicState()
{
    if (!pTxCharacteristic)
        return;
    std::vector<uint8_t> p = {0x10, (uint8_t)AppState.currentMode, AppState.brightness, (uint8_t)(AppState.autoReconnect ? 1 : 0)};
    pTxCharacteristic->setValue(p.data(), p.size());
    pTxCharacteristic->notify();
}

void WebGateway_BroadcastCdownConfig()
{
    if (!pTxCharacteristic)
        return;
    uint8_t mins = AppState.countdownTotalSeconds / 60;
    std::vector<uint8_t> p = {0x12, mins};
    pTxCharacteristic->setValue(p.data(), p.size());
    pTxCharacteristic->notify();
}

void WebGateway_BroadcastAlarmState(uint8_t idx)
{
    if (!pTxCharacteristic)
        return;
    AlarmData &a = AppState.alarms[idx];
    std::vector<uint8_t> p = {0x11, idx, (uint8_t)(a.isSet ? 1 : 0), (uint8_t)(a.enabled ? 1 : 0), a.hour, a.minute};
    pTxCharacteristic->setValue(p.data(), p.size());
    pTxCharacteristic->notify();
}

void WebGateway_BroadcastSavedDevices()
{
    if (!pTxCharacteristic)
        return;

    std::string macH = AppState.savedHrmMac;
    std::vector<uint8_t> pH = {0x16, 1, (uint8_t)macH.length()};
    for (char c : macH)
        pH.push_back(c);
    pTxCharacteristic->setValue(pH.data(), pH.size());
    pTxCharacteristic->notify();
    delay(20);

    std::string macC = AppState.savedCscMac;
    std::vector<uint8_t> pC = {0x16, 2, (uint8_t)macC.length()};
    for (char c : macC)
        pC.push_back(c);
    pTxCharacteristic->setValue(pC.data(), pC.size());
    pTxCharacteristic->notify();
}