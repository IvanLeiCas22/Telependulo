#ifndef CAMERAUSB_H
#define CAMERAUSB_H

#include "camera.h"

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

    bool capture(cv::Mat& frame) const override;

private:
    UsbCameraConfig config_;
};

#endif // CAMERAUSB_H
