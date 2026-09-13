#ifndef CAMERAIP_H
#define CAMERAIP_H

#include "camera.h"

#include <string>

struct IpCameraConfig
{
    std::string url;
    int timeoutMs = 10000;
};

class CameraIp : public Camera
{
public:
    explicit CameraIp(const IpCameraConfig& config);

    bool capture(cv::Mat& frame) const override;

private:
    IpCameraConfig config_;
};

#endif // CAMERAIP_H
