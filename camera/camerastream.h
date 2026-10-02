#ifndef CAMERASTREAM_H
#define CAMERASTREAM_H

#include "camera.h"

#include <condition_variable>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>

class CameraStream
{
public:
    CameraStream(Camera& camera, std::mutex& cameraMutex);
    ~CameraStream();

    bool start();
    bool latest(std::vector<unsigned char>& jpeg, std::uint64_t& sequence);
    void stop();

private:
    void produce();
    void markFailed();

    Camera& camera_;
    std::mutex& cameraMutex_;

    std::mutex stateMutex_;
    std::condition_variable frameReady_;
    std::condition_variable lifecycleChanged_;
    std::thread producer_;

    std::vector<unsigned char> latestFrame_;
    std::uint64_t latestSequence_ = 0;
    std::size_t consumers_ = 0;

    bool stopRequested_ = false;
    bool stopping_ = false;
    bool failed_ = false;
};

#endif // CAMERASTREAM_H
