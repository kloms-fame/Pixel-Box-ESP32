#pragma once
#include <stdint.h>
#include <stdbool.h>

// 1. 定义事件来源
enum EventType
{
    EVT_NONE = 0,
    EVT_BTN,   // 按键输入
    EVT_VOICE, // 语音输入
    EVT_WEB    // Web 蓝牙输入
};

// 2. 定义行为意图 (Action)
enum ActionID
{
    ACT_NONE = 0,

    // 模式控制
    ACT_MODE_SWITCH, // 切换模式 (value = 目标 Mode)

    // 秒表控制
    ACT_TIMER_START, // 启动秒表
    ACT_TIMER_PAUSE, // 暂停秒表
    ACT_TIMER_RESET, // 重置秒表

    // 倒计时控制
    ACT_CDOWN_SET,   // 设定倒计时预设值 (value = 目标秒数)
    ACT_CDOWN_START, // 启动倒计时
    ACT_CDOWN_PAUSE, // 暂停倒计时
    ACT_CDOWN_RESET, // 重置倒计时 (恢复到预设值)

    // 闹钟控制
    ACT_ALARM_SET_INDEX, // 切换当前显示的闹钟组 (value = index)
    ACT_ALARM_TOGGLE,    // 翻转指定闹钟的开关状态 (value = index)
    ACT_ALARM_SAVE,      // 保存并开启闹钟 (value = 紧凑打包: (idx<<24)|(en<<16)|(h<<8)|m )
    ACT_ALARM_STOP_RING, // 停止当前所有响铃

    // 系统与硬件配置
    ACT_SYS_BRIGHTNESS, // 设置屏幕亮度 (value = 0~255)
    ACT_SYS_AUTOREC,    // 设置自动重连开关 (value = 0或1)

    // 传感器动作 (替换原来的 AppState.pendingCmd)
    ACT_SENSOR_CMD // 传感器底层指令 (value = 5~14对应原逻辑)
};

// 3. 事件消息体
struct EventMsg
{
    EventType type;
    ActionID action;
    uint32_t value; // 附带的参数值
};

// 全局接口
void Event_Init();
bool Event_Push(EventMsg msg);
bool Event_Pop(EventMsg *msg);