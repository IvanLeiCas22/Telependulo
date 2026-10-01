#ifndef CAMERA_H
#define CAMERA_H

#include <opencv2/core/mat.hpp>

class Camera
{
public:
    virtual ~Camera() = default;

    virtual bool capture(cv::Mat& frame) = 0;

    virtual bool open() { return true; }
    virtual bool read(cv::Mat& frame) { return capture(frame); }
    virtual void close() {}
};

#endif // CAMERA_H
