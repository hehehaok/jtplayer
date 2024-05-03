#include "threadpool.h"
#include <QDebug>

int ThreadPool::m_maxThreads;   // 线程池中的最大线程数
int ThreadPool::m_freeThreads;  // 空闲线程数
int ThreadPool::m_maxTasks;     // 任务队列中的最大任务数
int ThreadPool::m_pushIndex;    // 写入任务指针
int ThreadPool::m_readIndex;    // 读取任务指针
int ThreadPool::m_size;         // 当前任务队列中的任务数

int ThreadPool::m_initFlag = -1;                        // 线程池初始化标志，确保只初始化一次
std::vector<Thread> ThreadPool::m_threadQueue;     // 线程池
std::vector<Task> ThreadPool::m_taskQueue;         // 任务队列
std::mutex ThreadPool::m_mutex;                    // 互斥锁
std::condition_variable ThreadPool::m_cond;        // 条件变量


bool ThreadPool::init(int threadNum, int taskNum)
{
    if (m_initFlag != -1) {
        qDebug() << "threadPool has been init before!\n";
        return true;
    }
    m_pushIndex = 0; // 写指针位置初始化为0
    m_readIndex = 0; // 读指针位置初始化为0
    m_size = 0; // 任务数初始化为0
    m_maxThreads = threadNum; // 初始化最大线程数
    m_maxTasks = taskNum;     // 初始化最大任务数
    m_threadQueue.resize(m_maxThreads); // 初始化线程队列长度
    m_freeThreads = m_maxThreads; // 初始化空闲线程数
    m_taskQueue.resize(m_maxTasks); // 初始化任务队列长度
    for (int i = 0; i < m_maxThreads; i++) {
        m_threadQueue[i].isTerminate = false;
        m_threadQueue[i].isWorking = false;
        std::thread* _thread = new std::thread(threadEventLoop, (void*)&m_threadQueue[i]);
        if (_thread == nullptr) {
            qDebug() << "new thread fail!\n";
            return false;
        }
        m_threadQueue[i].id = _thread->get_id();
        _thread->detach(); // 线程分离，异步执行
    }
    m_initFlag = 1;
    return true;
}

bool ThreadPool::addTask(std::function<void(std::shared_ptr<void>)> func, std::shared_ptr<void> param)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_size >= m_maxTasks) {
        qDebug() << "the task queue is full, add task fail!\n";
        return false;
    }
    // 往任务队列的写指针处添加任务
    m_taskQueue[m_pushIndex].func = func;
    m_taskQueue[m_pushIndex].param = param;
    m_size++;
    m_pushIndex = (m_pushIndex + 1) % m_maxTasks; // 更新写指针
    m_cond.notify_one();
    return true;
}

void ThreadPool::releasePool()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    for (int i = 0; i < m_maxThreads; i++) {
        m_threadQueue[i].isTerminate = true;
    }
    m_initFlag = -1;
    lock.unlock();
    m_cond.notify_all();
    // 给50ms时间让各线程执行完毕后退出
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void ThreadPool::threadEventLoop(void *param)
{
    Thread* theThread = reinterpret_cast<Thread*>(param);
    while (true) {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (m_size == 0) {
            if (theThread->isTerminate) {
                break;
            }
            m_cond.wait(lock);
        }
        if (theThread->isTerminate) {
            break;
        }
        // 从任务队列的读指针处读取任务，并将队列中已读取任务注销，同时更新读指针
        Task task = m_taskQueue[m_readIndex];
        m_taskQueue[m_readIndex].func = nullptr;
        m_taskQueue[m_readIndex].param.reset();
        m_readIndex = (m_readIndex + 1) % m_maxTasks;
        m_size--;
        m_freeThreads--;
        lock.unlock();
        theThread->isWorking = true;
        task.func(task.param);
        theThread->isWorking = false;
        lock.lock();
        m_freeThreads++;
    }
}
