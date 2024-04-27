#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <memory>
#include <functional>

struct Task {
    std::function<void(std::shared_ptr<void>)> func; // 任务函数
    std::shared_ptr<void> param;                     // 任务函数参数
};

struct Thread {
    std::thread::id id; // 线程id
    bool isTerminate;   // 线程终止标志位
    bool isWorking;     // 线程工作标志位
};

class ThreadPool
{
public:
    static bool init(int threadNum = 6, int taskNum = 10); // 初始化线程池
    static bool addTask(std::function<void(std::shared_ptr<void>)> func, std::shared_ptr<void> param); // 往任务队列中添加任务
    static void releasePool();                  // 结束线程池并释放对应资源
    static void threadEventLoop(void* param);   // 每个线程的事件主循环

public:
    static int m_maxThreads;   // 线程池中的最大线程数
    static int m_freeThreads;  // 空闲线程数
    static int m_maxTasks;     // 任务队列中的最大任务数
    static int m_pushIndex;    // 写入任务指针
    static int m_readIndex;    // 读取任务指针
    static int m_size;         // 当前任务队列中的任务数

    static int m_initFlag;                        // 线程池初始化标志，确保只初始化一次
    static std::vector<Thread> m_threadQueue;     // 线程池
    static std::vector<Task> m_taskQueue;         // 任务队列
    static std::mutex m_mutex;                    // 互斥锁
    static std::condition_variable m_cond;        // 条件变量
};

#endif // THREADPOOL_H
