#include "TimeSync.h"
#include "GlobalState.h"
#include <WiFi.h>
#include "time.h"

void TimeSync_Init()
{
    // 🌟 核心修复1：无论是否配置了 WiFi，开机第一件事必须确立全局时区（东八区）
    // 这样底层的 localtime_r() 永远会统一、标准地 +8 小时
    configTime(8 * 3600, 0, "ntp.aliyun.com", "ntp.tencent.com", "pool.ntp.org");

    if (AppState.wifiSSID == "")
    {
        Serial.println("[🌐 NTP] 未配置 WiFi，跳过自动校时");
        return;
    }

    Serial.printf("[🌐 NTP] 正在尝试接入: [%s]\n", AppState.wifiSSID.c_str());

    // 【极其关键的底层清洗】：强制恢复出厂射频状态，清除任何 AP 模式残留
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, true);
    delay(200);

    WiFi.begin(AppState.wifiSSID.c_str(), AppState.wifiPass.c_str());

    // 放宽连接超时至 15 秒（由于部分手机热点分配 IP 极慢）
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30)
    {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\n[✅ WiFi] 成功接入网络，正在获取标准时间...");

        // 最长等待 5 秒获取时间
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 5000))
        {
            Serial.printf("[⏰ NTP] 校时成功: %04d-%02d-%02d %02d:%02d:%02d\n",
                          timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        }
        else
        {
            Serial.println("[⚠️ NTP] 获取时间失败，可能是连接的路由器无法访问公网");
        }
    }
    else
    {
        Serial.println("\n[⚠️ WiFi] 连接超时！请检查密码，或确保网络为纯 2.4GHz 频段。");
    }

    // 【极客保命符】：不论是否成功，彻底关闭 WiFi 硬件模块以节省电池
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    Serial.println("[🔌 PWR] WiFi 射频已完全关断，回归低功耗态");
}