#include "GlobalState.h"
#include <NimBLEDevice.h> // 【新增】必须引入此库以进行底层反初始化

void AppStateManager::begin()
{
    prefs.begin(NVS_NAMESPACE, false);
    brightness = prefs.getUChar("bright", BRIGHTNESS_DEFAULT);
    autoReconnect = prefs.getBool("autoRec", false);
    savedHrmMac = prefs.getString("macHrm", "").c_str();
    savedCscMac = prefs.getString("macCsc", "").c_str();

    // 【新增】开机从 NVS 读取 WiFi
    wifiSSID = prefs.getString("wssID", "").c_str();
    wifiPass = prefs.getString("wPass", "").c_str();

    for (int i = 0; i < 3; i++)
    {
        String prefix = "alm" + String(i);
        alarms[i].isSet = prefs.getBool((prefix + "_set").c_str(), false);
        alarms[i].enabled = prefs.getBool((prefix + "_en").c_str(), false);
        alarms[i].hour = prefs.getUChar((prefix + "_h").c_str(), 0);
        alarms[i].minute = prefs.getUChar((prefix + "_m").c_str(), 0);
    }
    Serial.println("[💾 NVS] 系统配置已从 NVS 持久化存储中恢复");
}

void AppStateManager::saveBrightness(uint8_t b)
{
    brightness = b;
    prefs.putUChar("bright", b);
}

void AppStateManager::saveAutoReconnect(bool enabled)
{
    autoReconnect = enabled;
    prefs.putBool("autoRec", enabled);
    Serial.printf("[💾 NVS] 自动重连设定为: %s\n", enabled ? "开启" : "关闭");
}

void AppStateManager::saveSensorMac(uint8_t type, string mac)
{
    if (type == 1)
    {
        savedHrmMac = mac;
        prefs.putString("macHrm", String(mac.c_str()));
        Serial.printf("[💾 NVS] 记忆心率带 MAC: %s\n", mac.c_str());
    }
    else if (type == 2)
    {
        savedCscMac = mac;
        prefs.putString("macCsc", String(mac.c_str()));
        Serial.printf("[💾 NVS] 记忆踏频器 MAC: %s\n", mac.c_str());
    }
}

void AppStateManager::saveAlarm(uint8_t idx, bool enabled, uint8_t h, uint8_t m)
{
    if (idx >= 3)
        return;
    alarms[idx].isSet = true;
    alarms[idx].enabled = enabled;
    alarms[idx].hour = h;
    alarms[idx].minute = m;
    String prefix = "alm" + String(idx);
    prefs.putBool((prefix + "_set").c_str(), true);
    prefs.putBool((prefix + "_en").c_str(), enabled);
    prefs.putUChar((prefix + "_h").c_str(), h);
    prefs.putUChar((prefix + "_m").c_str(), m);
    Serial.printf("[💾 NVS] 闹钟 %d 配置已保存\n", idx + 1);
}

void AppStateManager::clearSensorMac(uint8_t type)
{
    if (type == 1)
    {
        savedHrmMac = "";
        prefs.putString("macHrm", "");
        Serial.println("[💾 NVS] 已手动清除心率带 MAC 记忆");
    }
    else if (type == 2)
    {
        savedCscMac = "";
        prefs.putString("macCsc", "");
        Serial.println("[💾 NVS] 已手动清除踏频器 MAC 记忆");
    }
}

// 【新增】保存网络配置
void AppStateManager::saveWiFi(string ssid, string pass)
{
    wifiSSID = ssid;
    wifiPass = pass;
    prefs.putString("wssID", String(ssid.c_str()));
    prefs.putString("wPass", String(pass.c_str()));
    Serial.println("[💾 NVS] WiFi 凭据已安全存入闪存");
}

void AppStateManager::factoryReset()
{
    Serial.println("\n[🧨 FACTORY RESET] 收到出厂重置指令，正在擦除 NVS 存储...");
    prefs.clear();
    delay(100);
    NimBLEDevice::deinit(true);
    delay(500);
    Serial.println("[🧨 FACTORY RESET] 擦除完成，系统将在 1 秒后自动重启.");
    ESP.restart();
}