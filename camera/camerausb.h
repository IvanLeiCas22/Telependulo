#ifndef CAMERAUSB_H
#define CAMERAUSB_H

#include "camera.h"

#include <opencv2/videoio.hpp>

#include <string>
#include <vector>

struct V4l2MenuItem
{
    int value = 0;
    std::string name;
};

struct CameraMode
{
    int width = 0;
    int height = 0;
    double fps = 0.0;
};

struct V4l2Control
{
    unsigned int id = 0;
    std::string name;
    unsigned int type = 0;
    int value = 0;
    bool hasValue = false;
    long long min = 0;
    long long max = 0;
    long long step = 0;
    bool inactive = false;
    bool readOnly = false;
    std::vector<V4l2MenuItem> menuItems;
};

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

    bool getAvailableModes(std::vector<CameraMode>& modes) const;
    bool getV4l2Controls(std::vector<V4l2Control>& controls) const;
    bool setV4l2Control(unsigned int id, int value) const;

    bool capture(cv::Mat& frame) override;

private:
    UsbCameraConfig config_;
    cv::VideoCapture camera_;
    int users_ = 0;
    bool contratoMostrado_ = false;
};

#endif // CAMERAUSB_H
