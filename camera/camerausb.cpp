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

    const int yuyv = cv::VideoWriter::fourcc('Y', 'U', 'Y', 'V');

    camera.set(cv::CAP_PROP_FOURCC, yuyv);
    camera.set(cv::CAP_PROP_FRAME_WIDTH, config_.width);
    camera.set(cv::CAP_PROP_FRAME_HEIGHT, config_.height);
    camera.set(cv::CAP_PROP_FPS, config_.fps);

    if (!camera.read(frame) || frame.empty())
    {
        std::cerr << "[CameraUsb] No se pudo capturar una imagen.\n";
        return false;
    }

    return true;
}
