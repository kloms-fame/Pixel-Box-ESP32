#include "SensorHub.h"
#include "GlobalState.h"
#include "DisplayCore.h"
#include "WebGateway.h"
#include <vector>

std::vector<NimBLEClient *> activeClients;
ScanCallbackFunc activeScanCb = nullptr;
uint16_t lastCrankRevs = 0;
uint16_t lastCrankTime = 0;

// 【修复核心 1】：使用全局缓存变量替代极其耗时的 getService() 实时查询，消除切屏卡顿！
bool cachedHasHRM = false;
bool cachedHasCSC = false;

void updateSensorCache()
{
    cachedHasHRM = false;
    cachedHasCSC = false;
    for (auto pClient : activeClients)
    {
        if (pClient->isConnected())
        {
            if (pClient->getService(NimBLEUUID(UUID_HRM_SVC)) != nullptr)
                cachedHasHRM = true;
            if (pClient->getService(NimBLEUUID(UUID_CSC_SVC)) != nullptr)
                cachedHasCSC = true;
        }
    }
}

bool SensorHub_HasHRM() { return cachedHasHRM; }
bool SensorHub_HasCSC() { return cachedHasCSC; }

void notifySensorCallback(NimBLERemoteCharacteristic *pChar, uint8_t *pData, size_t length, bool isNotify)
{
    uint16_t charUUID = pChar->getUUID().getNative()->u16.value;
    uint8_t flags = pData[0];

    if (charUUID == UUID_HRM_CHAR)
    {
        AppState.currentHR = (flags & 1) ? (pData[1] | (pData[2] << 8)) : pData[1];
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
                uint16_t timeDiff = (uint16_t)(crankTime - lastCrankTime);
                uint16_t revsDiff = (uint16_t)(crankRevs - lastCrankRevs);
                if (timeDiff > 0)
                {
                    AppState.currentCadence = (uint16_t)(((uint32_t)revsDiff * 60 * 1024) / timeDiff);
                    AppState.lastCrankSysTime = millis();
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
        updateSensorCache(); // 更新缓存

        if (activeClients.empty())
        {
            AppState.currentHR = 0;
            AppState.currentCadence = 0;
            if (AppState.currentMode == MODE_SENSOR_HRM || AppState.currentMode == MODE_SENSOR_CSC)
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
                // 【修复核心 2】：防重复连接轰炸拦截器
                bool alreadyActive = false;
                for (auto c : activeClients)
                {
                    if (c->getPeerAddress() == device->getAddress())
                        alreadyActive = true;
                }
                // 只有在没连过，且指令队列没满的情况下才触发
                if (!alreadyActive && AppState.pendingCmd != 6)
                {
                    Serial.printf("[⚡ AUTO-RECONNECT] 发现设备 %s，立即刹停雷达并直连！\n", mac.c_str());
                    NimBLEDevice::getScan()->stop(); // 立即强行停止扫描！杜绝[SYNC]狂发
                    AppState.pendingAddr = device->getAddress();
                    AppState.pendingCmd = 6;
                }
            }
        }
    }
};
AutoReconnectScanCallbacks autoScanCb;

void SensorHub_StartScan(ScanCallbackFunc cb)
{
    activeScanCb = cb;
    NimBLEScan *pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(&proxyScanCb);
    pScan->setActiveScan(true);
    pScan->start(5, false);
    pScan->clearResults();
}

void SensorHub_TriggerAutoReconnect(bool force)
{
    if (!force && !AppState.autoReconnect)
        return;
    if (AppState.savedHrmMac == "" && AppState.savedCscMac == "")
        return;
    NimBLEScan *pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(&autoScanCb);
    pScan->setActiveScan(true);
    pScan->start(5, false);
    pScan->clearResults();
}

void SensorHub_Connect(NimBLEAddress addr)
{
    NimBLEClient *pClient = NimBLEDevice::createClient();
    pClient->setClientCallbacks(&clientCB, false);
    pClient->setConnectTimeout(10);
    pClient->setConnectionParams(12, 12, 0, 51);

    if (!pClient->connect(addr))
    {
        NimBLEDevice::deleteClient(pClient);
        return;
    }
    activeClients.push_back(pClient);

    NimBLERemoteService *pSvcHRM = pClient->getService(NimBLEUUID(UUID_HRM_SVC));
    NimBLERemoteService *pSvcCSC = pClient->getService(NimBLEUUID(UUID_CSC_SVC));

    updateSensorCache(); // 更新缓存

    if (pSvcCSC != nullptr && pSvcHRM == nullptr)
    {
        AppState.currentMode = MODE_SENSOR_CSC;
    }
    else
    {
        AppState.currentMode = MODE_SENSOR_HRM;
    }

    Display_Clear();
    Display_Show();
    WebGateway_BroadcastBasicState();

    if (pSvcHRM)
    {
        auto pChr = pSvcHRM->getCharacteristic(NimBLEUUID(UUID_HRM_CHAR));
        if (pChr && pChr->canNotify())
            pChr->subscribe(true, notifySensorCallback);
    }
    if (pSvcCSC)
    {
        auto pChr = pSvcCSC->getCharacteristic(NimBLEUUID(UUID_CSC_CHAR));
        if (pChr && pChr->canNotify())
            pChr->subscribe(true, notifySensorCallback);
    }
}

void SensorHub_Disconnect(NimBLEAddress addr)
{
    for (auto pClient : activeClients)
    {
        if (pClient->getPeerAddress() == addr)
        {
            pClient->disconnect();
            return;
        }
    }
}

void SensorHub_DisconnectAll()
{
    auto clientsCopy = activeClients;
    for (auto pClient : clientsCopy)
    {
        if (pClient->isConnected())
            pClient->disconnect();
    }
}

int SensorHub_GetActiveClientCount() { return activeClients.size(); }