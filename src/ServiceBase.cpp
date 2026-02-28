#include "ServiceBase.h"
#include <iostream>
#include <chrono>

ServiceBase::ServiceBase(const std::string& name)
    : m_name(name) {
}

ServiceBase::~ServiceBase() {
    stop();
    join();
}

void ServiceBase::start() {
    if (m_running.load()) {
        std::cerr << "[" << m_name << "] Service is already running" << std::endl;
        return;
    }

    m_running.store(true);
    m_thread = std::thread([this]() {
        m_threadId = std::this_thread::get_id();
        std::cout << "[" << m_name << "] Service thread started" << std::endl;
        run();
        std::cout << "[" << m_name << "] Service thread exited" << std::endl;
    });
}

void ServiceBase::stop() {
    if (!m_running.load()) {
        return;
    }

    m_running.store(false);
    m_taskCv.notify_all();  // 唤醒等待的线程
}

void ServiceBase::join() {
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void ServiceBase::processTasks() {
    std::unique_lock<std::mutex> lock(m_taskMutex);
    
    // 等待任务或停止信号
    m_taskCv.wait(lock, [this] {
        return !m_taskQueue.empty() || !m_running.load();
    });

    // 处理所有待处理的任务
    while (!m_taskQueue.empty() && m_running.load()) {
        auto task = std::move(m_taskQueue.front());
        m_taskQueue.pop();
        
        lock.unlock();
        try {
            task();
        } catch (const std::exception& e) {
            std::cerr << "[" << m_name << "] Task exception: " << e.what() << std::endl;
        }
        lock.lock();
    }
}

