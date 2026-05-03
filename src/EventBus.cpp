#include "EventBus.h"

#define QUEUE_SIZE 32

// 简单的静态环形队列
static EventMsg queue[QUEUE_SIZE];
static int head = 0;
static int tail = 0;

void Event_Init()
{
    head = 0;
    tail = 0;
}

bool Event_Push(EventMsg msg)
{
    int next = (tail + 1) % QUEUE_SIZE;
    if (next == head)
        return false; // 满

    queue[tail] = msg;
    tail = next;
    return true;
}

bool Event_Pop(EventMsg *msg)
{
    if (head == tail)
        return false; // 空

    *msg = queue[head];
    head = (head + 1) % QUEUE_SIZE;
    return true;
}