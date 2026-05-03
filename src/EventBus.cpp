#include "EventBus.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <Arduino.h>

// 队列句柄
static QueueHandle_t eventQueue = NULL;

// 初始化（必须在 setup() 调用）
void Event_Init()
{
    // 创建一个深度为 32 的消息队列
    eventQueue = xQueueCreate(32, sizeof(EventMsg));

    if (eventQueue == NULL)
    {
        Serial.println("[FATAL] EventBus 创建失败！系统可能内存不足。");
    }
    else
    {
        Serial.println("[SYS] EventBus 初始化成功 (FreeRTOS xQueue)");
    }
}

// 发送事件
bool Event_Push(EventMsg msg)
{
    if (!eventQueue)
        return false;

    // ESP32 特色安全判断：当前是否在硬件中断 (ISR) 的上下文中？
    if (xPortInIsrContext())
    {
        BaseType_t hpTaskWoken = pdFALSE;

        // 在中断中必须使用专属的 FromISR API，且不能阻塞
        BaseType_t res = xQueueSendFromISR(eventQueue, &msg, &hpTaskWoken);

        if (hpTaskWoken)
        {
            // 如果此事件唤醒了更高优先级的任务，立刻让出当前中断执行权
            portYIELD_FROM_ISR();
        }

        return res == pdPASS;
    }
    else
    {
        // 在普通任务中发送（例如 BLE Task 或 Loop Task）
        // 最后一个参数 0 代表“非阻塞”：如果队列满了直接丢弃，绝不卡死当前任务！
        return xQueueSend(eventQueue, &msg, 0) == pdPASS;
    }
}

// 接收事件
bool Event_Pop(EventMsg *msg)
{
    if (!eventQueue)
        return false;

    // 非阻塞接收：如果没有消息，立刻返回 false，让 loop() 继续往下跑
    return xQueueReceive(eventQueue, msg, 0) == pdPASS;
}