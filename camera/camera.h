#ifndef CAMERA_H
#define CAMERA_H

#include <opencv2/core/mat.hpp>

class Camera
{
public:
    virtual ~Camera() = default;
    virtual bool capture(cv::Mat& frame) const = 0;
};

#endif // CAMERA_H
