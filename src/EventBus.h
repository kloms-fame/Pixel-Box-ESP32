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
    ACT_MODE_SWITCH,    // 切换模式
    ACT_TIMER_START,    // 秒表开始
    ACT_TIMER_PAUSE,    // 秒表暂停
    ACT_TIMER_RESET,    // 秒表重置
    ACT_CDOWN_SET,      // 倒计时设定
    ACT_CDOWN_START,    // 倒计时开始
    ACT_CDOWN_PAUSE,    // 倒计时暂停
    ACT_CDOWN_RESET,    // 倒计时重置
    ACT_ALARM_TOGGLE,   // 闹钟开关
    ACT_BRIGHTNESS_SET, // 亮度调节
    ACT_SYS_CMD         // 其他系统指令 (寻呼等)
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