#include "camerausb.h"

#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <linux/videodev2.h>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>

namespace
{
const char* controlTypeName(__u32 type)
{
    switch (type)
    {
    case V4L2_CTRL_TYPE_INTEGER: return "integer";
    case V4L2_CTRL_TYPE_BOOLEAN: return "boolean";
    case V4L2_CTRL_TYPE_MENU: return "menu";
    case V4L2_CTRL_TYPE_BUTTON: return "button";
    case V4L2_CTRL_TYPE_INTEGER64: return "integer64";
    case V4L2_CTRL_TYPE_STRING: return "string";
    case V4L2_CTRL_TYPE_BITMASK: return "bitmask";
    case V4L2_CTRL_TYPE_INTEGER_MENU: return "integer-menu";
    default: return "other";
    }
}

bool readControlValue(int fd, const v4l2_query_ext_ctrl& control, int& value)
{
    if (control.flags & V4L2_CTRL_FLAG_WRITE_ONLY)
        return false;

    if (control.type == V4L2_CTRL_TYPE_BUTTON ||
        control.type == V4L2_CTRL_TYPE_INTEGER64 ||
        control.type == V4L2_CTRL_TYPE_STRING)
    {
        return false;
    }

    v4l2_control current{};
    current.id = control.id;

    if (ioctl(fd, VIDIOC_G_CTRL, &current) < 0)
        return false;

    value = current.value;
    return true;
}

void printMenu(int fd, const v4l2_query_ext_ctrl& control)
{
    for (__s64 index = control.minimum; index <= control.maximum; ++index)
    {
        v4l2_querymenu menu{};
        menu.id = control.id;
        menu.index = static_cast<__u32>(index);

        if (ioctl(fd, VIDIOC_QUERYMENU, &menu) < 0)
            continue;

        std::cout << "      " << index << ": ";

        if (control.type == V4L2_CTRL_TYPE_INTEGER_MENU)
            std::cout << menu.value;
        else
            std::cout << reinterpret_cast<const char*>(menu.name);

        std::cout << "\n";
    }
}
}

CameraUsb::CameraUsb(const UsbCameraConfig& config) : config_(config)
{
}

bool CameraUsb::open()
{
    if (camera_.isOpened())
    {
        ++users_;
        return true;
    }

    if (!camera_.open(config_.deviceIndex, cv::CAP_V4L2))
    {
        std::cerr << "[CameraUsb] No se pudo abrir /dev/video" << config_.deviceIndex << ".\n";
        return false;
    }

    camera_.set(cv::CAP_PROP_FOURCC, config_.fourcc);
    camera_.set(cv::CAP_PROP_FRAME_WIDTH, config_.width);
    camera_.set(cv::CAP_PROP_FRAME_HEIGHT, config_.height);
    camera_.set(cv::CAP_PROP_FPS, config_.fps);

    int warmupFrames = static_cast<int>(camera_.get(cv::CAP_PROP_FPS));
    if (warmupFrames <= 0)
        warmupFrames = config_.fps;

    cv::Mat warmupFrame;
    for (int i = 0; i < warmupFrames; ++i)
    {
        if (!camera_.read(warmupFrame) || warmupFrame.empty())
        {
            std::cerr << "[CameraUsb] Fallo durante el warm-up.\n";
            camera_.release();
            return false;
        }
    }

    users_ = 1;
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
    if (users_ <= 0)
        return;

    --users_;

    if (users_ == 0 && camera_.isOpened())
        camera_.release();
}

bool CameraUsb::isOpen() const
{
    return camera_.isOpened();
}

bool CameraUsb::setV4l2Control(unsigned int id, int value) const
{
    const std::string device = "/dev/video" + std::to_string(config_.deviceIndex);
    const int fd = ::open(device.c_str(), O_RDWR);

    if (fd < 0)
    {
        std::cerr << "[V4L2] No se pudo abrir " << device << " para cambiar un control.\n";
        return false;
    }

    v4l2_control control{};
    control.id = id;
    control.value = value;

    const bool success = ioctl(fd, VIDIOC_S_CTRL, &control) == 0;

    if (!success)
        std::cerr << "[V4L2] No se pudo cambiar el control 0x"
                  << std::hex << id << std::dec << " a " << value << ".\n";

    ::close(fd);
    return success;
}

void CameraUsb::printV4l2Controls() const
{
    const std::string device = "/dev/video" + std::to_string(config_.deviceIndex);
    const int fd = ::open(device.c_str(), O_RDWR);

    if (fd < 0)
    {
        std::cerr << "[V4L2] No se pudo abrir " << device << " para consultar controles.\n";
        return;
    }

    std::cout << "[V4L2] Controles de " << device << ":\n";

    v4l2_query_ext_ctrl control{};
    control.id = V4L2_CTRL_FLAG_NEXT_CTRL;

    bool found = false;

    while (ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &control) == 0)
    {
        if (!(control.flags & V4L2_CTRL_FLAG_DISABLED) && control.type != V4L2_CTRL_TYPE_CTRL_CLASS)
        {
            found = true;

            int value = 0;
            const bool hasValue = readControlValue(fd, control, value);

            std::cout << "  " << reinterpret_cast<const char*>(control.name)
                      << " | id=0x" << std::hex << control.id << std::dec
                      << " | " << controlTypeName(control.type);

            if (hasValue)
                std::cout << " | value=" << value;

            std::cout << " | min=" << control.minimum
                      << " max=" << control.maximum
                      << " step=" << control.step
                      << " default=" << control.default_value;

            if (control.flags & V4L2_CTRL_FLAG_READ_ONLY) std::cout << " | read-only";
            if (control.flags & V4L2_CTRL_FLAG_INACTIVE) std::cout << " | inactive";
            std::cout << "\n";

            if (control.type == V4L2_CTRL_TYPE_MENU || control.type == V4L2_CTRL_TYPE_INTEGER_MENU)
                printMenu(fd, control);
        }

        control.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }

    if (!found)
        std::cout << "  No se encontraron controles.\n";
    else if (errno != EINVAL)
        std::cerr << "[V4L2] La enumeracion termino con un error.\n";

    ::close(fd);
}

bool CameraUsb::capture(cv::Mat& frame)
{
    if (!open())
        return false;

    const bool success = read(frame);
    close();

    return success;
}
