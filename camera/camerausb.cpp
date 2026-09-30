#include "camerausb.h"

#include <iostream>
#include <opencv2/videoio.hpp>

CameraUsb::CameraUsb(const UsbCameraConfig& config) : config_(config)
{
}

bool CameraUsb::capture(cv::Mat& frame) const
{
    frame.release();

    cv::VideoCapture camera;
    if (!camera.open(config_.deviceIndex, cv::CAP_V4L2))
    {
        std::cerr << "[CameraUsb] No se pudo abrir /dev/video" << config_.deviceIndex << ".\n";
        return false;
    }

    camera.set(cv::CAP_PROP_FOURCC, config_.fourcc);
    camera.set(cv::CAP_PROP_FRAME_WIDTH, config_.width);
    camera.set(cv::CAP_PROP_FRAME_HEIGHT, config_.height);
    camera.set(cv::CAP_PROP_FPS, config_.fps);

    if (!camera.read(frame) || frame.empty())
    {
        std::cerr << "[CameraUsb] No se pudo capturar una imagen.\n";
        return false;
    }

    if (!contratoMostrado_)
    {
        const int fourcc = static_cast<int>(camera.get(cv::CAP_PROP_FOURCC));
        const char formato[] = {
            static_cast<char>(fourcc & 0xFF),
            static_cast<char>((fourcc >> 8) & 0xFF),
            static_cast<char>((fourcc >> 16) & 0xFF),
            static_cast<char>((fourcc >> 24) & 0xFF),
            '\0'
        };

        std::cout << "[CameraUsb] /dev/video" << config_.deviceIndex
                  << " - " << formato
                  << " | " << camera.get(cv::CAP_PROP_FRAME_WIDTH)
                  << "x" << camera.get(cv::CAP_PROP_FRAME_HEIGHT)
                  << " | " << camera.get(cv::CAP_PROP_FPS) << " FPS\n";

        contratoMostrado_ = true;
    }

    return true;
}
