#include "CaptureThread.h"

CaptureThread::CaptureThread(int width, int height) : buffer1(width, height), buffer2(width, height), running(false), isBuffer1InUse(ture) {}



CaptureThread::start() {
    running.store(true);
    captureThread = std::make_unique<std::thread>(&CaptureThread::captureLoop, this);
}


void CaptureThread::stop() {
    running.store(false);
    if (captureThread && captureThread->joinable()) {
        captureThread->join();
    }
}