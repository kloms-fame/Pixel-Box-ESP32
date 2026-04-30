#include "SensorHub.h"
#include "GlobalState.h"
#include "DisplayCore.h"
#include <vector>

std::vector<NimBLEClient *> activeClients;
ScanCallbackFunc activeScanCb = nullptr;
uint16_t lastCrankRevs = 0;
uint16_t lastCrankTime = 0;

void notifySensorCallback(NimBLERemoteCharacteristic *pChar, uint8_t *pData, size_t length, bool isNotify)
{
    uint16_t charUUID = pChar->getUUID().getNative()->u16.value;
    uint8_t flags = pData[0];

    Serial.printf("\n[📥 SENSOR RAW] UUID: 0x%04X, Len: %d | HEX: ", charUUID, length);
    for (size_t i = 0; i < length; i++)
        Serial.printf("%02X ", pData[i]);
    Serial.println();

    if (charUUID == UUID_HRM_CHAR)
    {
        AppState.currentHR = (flags & 1) ? (pData[1] | (pData[2] << 8)) : pData[1];
        Serial.printf("[❤ HRM PARSED] 当前心率: %d bpm\n", AppState.currentHR);
    }
    else if (charUUID == UUID_CSC_CHAR)
    {
        int offset = (flags & 1) ? 5 : 1;
        if (flags & 2)
        {
            uint16_t crankRevs = pData[offset] | (pData[offset + 1] << 8);
            uint16_t crankTime = pData[offset + 2] | (pData[offset + 3] << 8);
            if (lastCrankTime != 0 && crankTime != lastCrankTime)
            {
                // 解决 65533 塌陷问题
                uint16_t timeDiff = (uint16_t)(crankTime - lastCrankTime);
                uint16_t revsDiff = (uint16_t)(crankRevs - lastCrankRevs);
                if (timeDiff > 0)
                {
                    AppState.currentCadence = (uint16_t)(((uint32_t)revsDiff * 60 * 1024) / timeDiff);
                    AppState.lastCrankSysTime = millis();
                    Serial.printf("[🚴 CSC PARSED] 踏频更新: %d rpm (增量: %u 圈 / %u 单位时间)\n", AppState.currentCadence, revsDiff, timeDiff);
                }
            }
            lastCrankRevs = crankRevs;
            lastCrankTime = crankTime;
        }
    }
}

class ClientCallbacks : public NimBLEClientCallbacks
{
    void onConnect(NimBLEClient *pClient) override
    {
        Serial.printf("[✅ CONNECTED] 已连接外设: %s\n", pClient->getPeerAddress().toString().c_str());
    }
    void onDisconnect(NimBLEClient *pClient) override
    {
        Serial.printf("[❌ DISCONNECTED] 外设断开: %s\n", pClient->getPeerAddress().toString().c_str());
        for (auto it = activeClients.begin(); it != activeClients.end();)
        {
            if (*it == pClient)
                it = activeClients.erase(it);
            else
                ++it;
        }
        if (activeClients.empty())
        {
            AppState.currentHR = 0;
            AppState.currentCadence = 0;
            if (AppState.currentMode == MODE_SENSOR)
            {
                Display_Clear();
                Display_Show();
            }
            Serial.println("[🧹 AUTO CLEAN] 所有外设已离线，清空数据缓存");
        }
    }
};
ClientCallbacks clientCB;

class ProxyScanCallbacks : public NimBLEAdvertisedDeviceCallbacks
{
    void onResult(NimBLEAdvertisedDevice *device)
    {
        if (device->haveName() && device->getName().length() > 0)
        {
            uint8_t type = 0;
            if (device->isAdvertisingService(NimBLEUUID(UUID_HRM_SVC)))
                type = 1;
            else if (device->isAdvertisingService(NimBLEUUID(UUID_CSC_SVC)))
                type = 2;

            Serial.printf("[📡 SCAN HIT] MAC: %s, Name: %s\n", device->getAddress().toString().c_str(), device->getName().c_str());
            if (activeScanCb)
                activeScanCb(type, device->getAddress().getType(), device->getAddress().toString(), device->getName());
        }
    }
};
ProxyScanCallbacks proxyScanCb;

class AutoReconnectScanCallbacks : public NimBLEAdvertisedDeviceCallbacks
{
    void onResult(NimBLEAdvertisedDevice *device)
    {
        string mac = device->getAddress().toString();
        if (AppState.autoReconnect)
        {
            if (mac == AppState.savedHrmMac || mac == AppState.savedCscMac)
            {
                Serial.printf("[⚡ AUTO-RECONNECT] 发现历史设备 %s，即将直连！\n", mac.c_str());
                AppState.pendingAddr = device->getAddress();
                AppState.pendingCmd = 6;
            }
        }
    }
};
AutoReconnectScanCallbacks autoScanCb;

void SensorHub_StartScan(ScanCallbackFunc cb)
{
    Serial.println("\n[🔍 SYS_SCAN] 启动物理层广域扫描...");
    activeScanCb = cb;
    NimBLEScan *pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(&proxyScanCb);
    pScan->setActiveScan(true);
    pScan->start(5, false);
    pScan->clearResults();
    Serial.println("[🔍 SYS_SCAN] 物理扫描结束.");
}

void SensorHub_TriggerAutoReconnect()
{
    if (!AppState.autoReconnect || (AppState.savedHrmMac == "" && AppState.savedCscMac == ""))
    {
        Serial.println("[⏭️ AUTO-RECONNECT] 未开启自动重连或无历史记录，跳过.");
        return;
    }
    Serial.println("\n[🔍 AUTO-RECONNECT] 后台寻呼历史绑定设备中...");
    NimBLEScan *pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(&autoScanCb);
    pScan->setActiveScan(true);
    pScan->start(5, false);
    pScan->clearResults();
}

void SensorHub_Connect(NimBLEAddress addr)
{
    Serial.printf("\n[🔗 CONNECT INIT] 尝试直连 MAC: %s\n", addr.toString().c_str());
    NimBLEClient *pClient = NimBLEDevice::createClient();
    pClient->setClientCallbacks(&clientCB, false);
    pClient->setConnectTimeout(10);
    pClient->setConnectionParams(12, 12, 0, 51);

    if (!pClient->connect(addr))
    {
        Serial.printf("[⚠️ EXCEPTION] 连接失败! 设备可能未唤醒 (MAC: %s)\n", addr.toString().c_str());
        NimBLEDevice::deleteClient(pClient);
        return;
    }
    activeClients.push_back(pClient);
    AppState.currentMode = MODE_SENSOR;
    Display_Clear();
    Display_Show();

    NimBLERemoteService *pSvcHRM = pClient->getService(NimBLEUUID(UUID_HRM_SVC));
    if (pSvcHRM)
    {
        auto pChr = pSvcHRM->getCharacteristic(NimBLEUUID(UUID_HRM_CHAR));
        if (pChr && pChr->canNotify())
            pChr->subscribe(true, notifySensorCallback);
    }
    NimBLERemoteService *pSvcCSC = pClient->getService(NimBLEUUID(UUID_CSC_SVC));
    if (pSvcCSC)
    {
        auto pChr = pSvcCSC->getCharacteristic(NimBLEUUID(UUID_CSC_CHAR));
        if (pChr && pChr->canNotify())
            pChr->subscribe(true, notifySensorCallback);
    }
}

void SensorHub_Disconnect(NimBLEAddress addr)
{
    Serial.printf("\n[✂️ DISCONNECT INIT] 强制切断 MAC: %s\n", addr.toString().c_str());
    for (auto pClient : activeClients)
    {
        if (pClient->getPeerAddress() == addr)
        {
            pClient->disconnect();
            return;
        }
    }
}

int SensorHub_GetActiveClientCount() { return activeClients.size(); }
// ... 前面的代码不变 ...

void SensorHub_DisconnectAll()
{
    Serial.println("\n[✂️ DISCONNECT ALL] 执行强制断开所有外设链路");
    // 【核心修复】：先复制一份数组，防止底层断开回调触发 erase 导致野指针崩溃重启
    auto clientsCopy = activeClients;
    for (auto pClient : clientsCopy)
    {
        if (pClient->isConnected())
        {
            pClient->disconnect();
        }
    }
}

void SensorHub_TriggerAutoReconnect(bool force)
{
    // 如果不是强行触发且自动重连关闭了，就跳过
    if (!force && !AppState.autoReconnect)
    {
        Serial.println("[⏭️ AUTO-RECONNECT] 未开启自动重连，跳过.");
        return;
    }
    if (AppState.savedHrmMac == "" && AppState.savedCscMac == "")
    {
        Serial.println("[⏭️ AUTO-RECONNECT] 数据库无历史设备记忆，无法直连.");
        return;
    }
    Serial.println("\n[🔍 AUTO-RECONNECT] 正在后台寻呼已知绑定设备...");
    NimBLEScan *pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(&autoScanCb);
    pScan->setActiveScan(true);
    pScan->start(5, false);
    pScan->clearResults();
}