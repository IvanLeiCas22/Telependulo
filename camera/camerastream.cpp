#include "camerastream.h"

#include <opencv2/imgcodecs.hpp>

#include <utility>

CameraStream::CameraStream(Camera& camera, std::mutex& cameraMutex)
    : camera_(camera), cameraMutex_(cameraMutex)
{
}

CameraStream::~CameraStream()
{
    std::thread producer;
    bool closeCamera = false;

    {
        std::lock_guard<std::mutex> lock(stateMutex_);

        if (producer_.joinable())
        {
            consumers_ = 0;
            stopping_ = true;
            stopRequested_ = true;
            frameReady_.notify_all();

            producer = std::move(producer_);
            closeCamera = true;
        }
    }

    if (producer.joinable())
        producer.join();

    if (closeCamera)
    {
        std::lock_guard<std::mutex> lock(cameraMutex_);
        camera_.close();
    }
}

bool CameraStream::start()
{
    std::unique_lock<std::mutex> lock(stateMutex_);

    lifecycleChanged_.wait(lock, [this]()
    {
        return !stopping_;
    });

    if (consumers_ > 0)
    {
        if (failed_)
            return false;

        ++consumers_;
        return true;
    }

    {
        std::lock_guard<std::mutex> cameraLock(cameraMutex_);

        if (!camera_.open())
            return false;
    }

    stopRequested_ = false;
    failed_ = false;
    latestFrame_.clear();
    latestSequence_ = 0;
    consumers_ = 1;

    producer_ = std::thread(&CameraStream::produce, this);
    return true;
}

bool CameraStream::latest(std::vector<unsigned char>& jpeg, std::uint64_t& sequence)
{
    std::unique_lock<std::mutex> lock(stateMutex_);

    frameReady_.wait(lock, [this, sequence]()
    {
        return stopRequested_ || failed_ || latestSequence_ != sequence;
    });

    if (stopRequested_)
        return false;

    if (latestSequence_ == sequence)
        return false;

    jpeg = latestFrame_;
    sequence = latestSequence_;
    return true;
}

void CameraStream::stop()
{
    std::thread producer;

    {
        std::lock_guard<std::mutex> lock(stateMutex_);

        if (consumers_ == 0)
            return;

        --consumers_;

        if (consumers_ > 0)
            return;

        stopping_ = true;
        stopRequested_ = true;
        frameReady_.notify_all();

        producer = std::move(producer_);
    }

    if (producer.joinable())
        producer.join();

    {
        std::lock_guard<std::mutex> lock(cameraMutex_);
        camera_.close();
    }

    {
        std::lock_guard<std::mutex> lock(stateMutex_);

        latestFrame_.clear();
        latestSequence_ = 0;
        failed_ = false;
        stopRequested_ = false;
        stopping_ = false;
    }

    lifecycleChanged_.notify_all();
}

void CameraStream::produce()
{
    while (true)
    {
        {
            std::lock_guard<std::mutex> lock(stateMutex_);

            if (stopRequested_)
                break;
        }

        cv::Mat frame;

        {
            std::lock_guard<std::mutex> lock(cameraMutex_);

            if (!camera_.read(frame))
            {
                markFailed();
                return;
            }
        }

        std::vector<unsigned char> jpeg;
        if (!cv::imencode(".jpg", frame, jpeg))
        {
            markFailed();
            return;
        }

        {
            std::lock_guard<std::mutex> lock(stateMutex_);

            if (stopRequested_)
                break;

            latestFrame_ = std::move(jpeg);
            ++latestSequence_;
        }

        frameReady_.notify_all();
    }
}

void CameraStream::markFailed()
{
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        failed_ = true;
    }

    frameReady_.notify_all();
}
