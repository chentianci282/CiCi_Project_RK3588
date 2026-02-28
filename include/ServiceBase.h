#ifndef SERVICE_BASE_H
#define SERVICE_BASE_H

#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <queue>
#include <condition_variable>

/**
 * @brief 服务基类
 * 
 * 所有服务线程的基类，提供：
 * - 线程生命周期管理
 * - 任务投递机制
 * - 线程安全的状态管理
 */
class ServiceBase {
public:
    ServiceBase(const std::string& name);
    virtual ~ServiceBase();

    // 禁止拷贝
    ServiceBase(const ServiceBase&) = delete;
    ServiceBase& operator=(const ServiceBase&) = delete;

    /**
     * @brief 启动服务线程
     */
    void start();

    /**
     * @brief 停止服务线程
     */
    void stop();

    /**
     * @brief 等待线程退出
     */
    void join();

    /**
     * @brief 检查服务是否运行中
     */
    bool isRunning() const { return m_running.load(); }

    /**
     * @brief 检查当前是否在服务线程中
     */
    bool isInServiceThread() const {
        return std::this_thread::get_id() == m_threadId;
    }

    /**
     * @brief 投递任务到服务线程
     * 
     * @param f 要执行的任务（函数对象）
     */
    template<typename F>
    void post(F&& f) {
        {
            std::lock_guard<std::mutex> lock(m_taskMutex);
            m_taskQueue.push(std::forward<F>(f));
        }
        m_taskCv.notify_one();
    }

    /**
     * @brief 同步投递任务（投递并等待完成）
     * 
     * @param f 要执行的任务
     */
    template<typename F>
    void postSync(F&& f) {
        std::mutex syncMutex;
        std::condition_variable syncCv;
        bool done = false;

        post([&]() {
            f();
            {
                std::lock_guard<std::mutex> lock(syncMutex);
                done = true;
            }
            syncCv.notify_one();
        });

        std::unique_lock<std::mutex> lock(syncMutex);
        syncCv.wait(lock, [&] { return done; });
    }

protected:
    /**
     * @brief 服务线程主循环（子类实现）
     */
    virtual void run() = 0;

    /**
     * @brief 处理任务队列
     */
    void processTasks();

    /**
     * @brief 服务名称（用于日志）
     */
    std::string m_name;

    /**
     * @brief 运行标志
     */
    std::atomic<bool> m_running{false};

private:
    /**
     * @brief 线程对象
     */
    std::thread m_thread;

    /**
     * @brief 线程ID
     */
    std::thread::id m_threadId;

    /**
     * @brief 任务队列
     */
    std::queue<std::function<void()>> m_taskQueue;

    /**
     * @brief 任务队列互斥锁
     */
    std::mutex m_taskMutex;

    /**
     * @brief 任务队列条件变量
     */
    std::condition_variable m_taskCv;
};

#endif // SERVICE_BASE_H

