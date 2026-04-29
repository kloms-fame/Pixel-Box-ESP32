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

    // 【Debug】底层的每一次心跳报文都在此监控
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
                // 【核心修复】强制类型转换为 uint16_t，利用无符号数溢出特性完美解决 Rollover 翻转问题！
                uint16_t timeDiff = (uint16_t)(crankTime - lastCrankTime);
                uint16_t revsDiff = (uint16_t)(crankRevs - lastCrankRevs);

                if (timeDiff > 0)
                {
                    // 先转为 32位 运算防止乘法阶段溢出
                    AppState.currentCadence = (uint16_t)(((uint32_t)revsDiff * 60 * 1024) / timeDiff);
                    AppState.lastCrankSysTime = millis();
                    Serial.printf("[🚴 CSC PARSED] 踏频更新: %d rpm (圈数增量: %u, 时间增量: %u)\n", AppState.currentCadence, revsDiff, timeDiff);
                }
            }
            else if (crankTime == lastCrankTime)
            {
                // 曲柄时间没变，说明轮子没转 (通常发生在惯性滑行或完全停止时)
                Serial.println("[🚴 CSC STATIC] 传感器已发送数据，但曲柄时间戳未变 (滑行/静止中)");
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
        Serial.printf("[✅ CLIENT CONNECTED] 已成功连接至设备: %s\n", pClient->getPeerAddress().toString().c_str());
    }
    void onDisconnect(NimBLEClient *pClient) override
    {
        Serial.printf("[❌ CLIENT DISCONNECTED] 设备连接已断开: %s\n", pClient->getPeerAddress().toString().c_str());
        for (auto it = activeClients.begin(); it != activeClients.end();)
        {
            if (*it == pClient)
                it = activeClients.erase(it);
            else
                ++it;
        }
        if (activeClients.empty())
        {
            Serial.println("[🧹 AUTO CLEAN] 所有外设已断开，自动清理数据缓存");
            AppState.currentHR = 0;
            AppState.currentCadence = 0;
            if (AppState.currentMode == MODE_SENSOR)
            {
                Display_Clear();
                Display_Show();
            }
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

            Serial.printf("[📡 SCAN HIT] MAC: %s, RSSI: %d, Type: %d, Name: %s\n",
                          device->getAddress().toString().c_str(), device->getRSSI(), type, device->getName().c_str());

            if (activeScanCb)
                activeScanCb(type, device->getAddress().getType(), device->getAddress().toString(), device->getName());
        }
    }
};
ProxyScanCallbacks proxyScanCb;

void SensorHub_StartScan(ScanCallbackFunc cb)
{
    Serial.println("\n[🔍 SYS_SCAN] 启动底层物理广播扫描...");
    activeScanCb = cb;
    NimBLEScan *pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(&proxyScanCb);
    pScan->setActiveScan(true);
    pScan->start(5, false);
    pScan->clearResults();
    Serial.println("[🔍 SYS_SCAN] 物理扫描周期结束.");
}

void SensorHub_Connect(NimBLEAddress addr)
{
    Serial.printf("\n[🔗 CONNECT INIT] 正在与 MAC: %s 建立物理链路...\n", addr.toString().c_str());
    NimBLEClient *pClient = NimBLEDevice::createClient();
    pClient->setClientCallbacks(&clientCB, false);
    pClient->setConnectTimeout(10);
    pClient->setConnectionParams(12, 12, 0, 51);

    if (!pClient->connect(addr))
    {
        Serial.printf("[⚠️ EXCEPTION] 物理连接建立失败! 请确认设备未被手机占用且处于唤醒状态 (MAC: %s)\n", addr.toString().c_str());
        NimBLEDevice::deleteClient(pClient);
        return;
    }

    activeClients.push_back(pClient);
    AppState.currentMode = MODE_SENSOR;
    Display_Clear();
    Display_Show(); // 立刻清屏过渡

    NimBLERemoteService *pSvcHRM = pClient->getService(NimBLEUUID(UUID_HRM_SVC));
    if (pSvcHRM)
    {
        auto pChr = pSvcHRM->getCharacteristic(NimBLEUUID(UUID_HRM_CHAR));
        if (pChr && pChr->canNotify())
        {
            pChr->subscribe(true, notifySensorCallback);
            Serial.println("[✅ SUBSCRIBE] 订阅 HRM 心率数据流成功");
        }
    }
    NimBLERemoteService *pSvcCSC = pClient->getService(NimBLEUUID(UUID_CSC_SVC));
    if (pSvcCSC)
    {
        auto pChr = pSvcCSC->getCharacteristic(NimBLEUUID(UUID_CSC_CHAR));
        if (pChr && pChr->canNotify())
        {
            pChr->subscribe(true, notifySensorCallback);
            Serial.println("[✅ SUBSCRIBE] 订阅 CSC 踏频数据流成功");
        }
    }
}

void SensorHub_Disconnect(NimBLEAddress addr)
{
    Serial.printf("\n[✂️ DISCONNECT INIT] 执行强制断开 MAC: %s\n", addr.toString().c_str());
    for (auto pClient : activeClients)
    {
        if (pClient->getPeerAddress() == addr)
        {
            pClient->disconnect();
            return;
        }
    }
    Serial.println("[⚠️ EXCEPTION] 未找到该设备的活动链路");
}

int SensorHub_GetActiveClientCount() { return activeClients.size(); }