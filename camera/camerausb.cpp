#include "camerausb.h"

#include <iostream>

CameraUsb::CameraUsb(const UsbCameraConfig& config) : config_(config)
{
}

bool CameraUsb::open()
{
    if (camera_.isOpened())
        return true;

    if (!camera_.open(config_.deviceIndex, cv::CAP_V4L2))
    {
        std::cerr << "[CameraUsb] No se pudo abrir /dev/video" << config_.deviceIndex << ".\n";
        return false;
    }

    camera_.set(cv::CAP_PROP_FOURCC, config_.fourcc);
    camera_.set(cv::CAP_PROP_FRAME_WIDTH, config_.width);
    camera_.set(cv::CAP_PROP_FRAME_HEIGHT, config_.height);
    camera_.set(cv::CAP_PROP_FPS, config_.fps);

    return true;
}

bool CameraUsb::read(cv::Mat& frame)
{
    frame.release();

    if (!camera_.isOpened())
    {
        std::cerr << "[CameraUsb] La camara no esta abierta.\n";
        return false;
    }

    if (!camera_.read(frame) || frame.empty())
    {
        std::cerr << "[CameraUsb] No se pudo capturar una imagen.\n";
        return false;
    }

    if (!contratoMostrado_)
    {
        const int fourcc = static_cast<int>(camera_.get(cv::CAP_PROP_FOURCC));
        const char formato[] = {
            static_cast<char>(fourcc & 0xFF),
            static_cast<char>((fourcc >> 8) & 0xFF),
            static_cast<char>((fourcc >> 16) & 0xFF),
            static_cast<char>((fourcc >> 24) & 0xFF),
            '\0'
        };

        std::cout << "[CameraUsb] /dev/video" << config_.deviceIndex
                  << " - " << formato
                  << " | " << camera_.get(cv::CAP_PROP_FRAME_WIDTH)
                  << "x" << camera_.get(cv::CAP_PROP_FRAME_HEIGHT)
                  << " | " << camera_.get(cv::CAP_PROP_FPS) << " FPS\n";

        contratoMostrado_ = true;
    }

    return true;
}

void CameraUsb::close()
{
    if (camera_.isOpened())
        camera_.release();
}

bool CameraUsb::isOpen() const
{
    return camera_.isOpened();
}

bool CameraUsb::capture(cv::Mat& frame)
{
    if (!open())
        return false;

    const bool success = read(frame);
    close();

    return success;
}
