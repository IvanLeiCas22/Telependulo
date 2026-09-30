#ifndef CAMERAUSB_H
#define CAMERAUSB_H

#include "camera.h"

#include <opencv2/videoio.hpp>

struct UsbCameraConfig
{
    int deviceIndex = 0;
    int width = 1920;
    int height = 1080;
    int fps = 5;
    int fourcc = 0;
};

class CameraUsb : public Camera
{
public:
    explicit CameraUsb(const UsbCameraConfig& config);

    bool open();
    bool read(cv::Mat& frame);
    void close();
    bool isOpen() const;
    void printV4l2Controls() const;

    bool capture(cv::Mat& frame) override;

private:
    UsbCameraConfig config_;
    cv::VideoCapture camera_;
    int users_ = 0;
    bool contratoMostrado_ = false;
};

#endif // CAMERAUSB_H
