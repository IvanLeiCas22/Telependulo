#include "camerausb.h"

#include <cerrno>
#include <fcntl.h>
#include <iostream>
#include <linux/videodev2.h>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include <utility>

namespace
{
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

std::vector<V4l2MenuItem> readMenuItems(int fd, const v4l2_query_ext_ctrl& control)
{
    std::vector<V4l2MenuItem> items;

    for (__s64 index = control.minimum; index <= control.maximum; ++index)
    {
        v4l2_querymenu menu{};
        menu.id = control.id;
        menu.index = static_cast<__u32>(index);

        if (ioctl(fd, VIDIOC_QUERYMENU, &menu) < 0)
            continue;

        V4l2MenuItem item;
        item.value = static_cast<int>(index);

        if (control.type == V4L2_CTRL_TYPE_INTEGER_MENU)
            item.name = std::to_string(menu.value);
        else
            item.name = reinterpret_cast<const char*>(menu.name);

        items.push_back(std::move(item));
    }

    return items;
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

bool CameraUsb::getV4l2Controls(std::vector<V4l2Control>& controls) const
{
    controls.clear();
    const std::string device = "/dev/video" + std::to_string(config_.deviceIndex);
    const int fd = ::open(device.c_str(), O_RDWR);

    if (fd < 0)
    {
        std::cerr << "[V4L2] No se pudo abrir " << device << " para consultar controles.\n";
        return false;
    }

    v4l2_query_ext_ctrl query{};
    query.id = V4L2_CTRL_FLAG_NEXT_CTRL;

    while (ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &query) == 0)
    {
        if (!(query.flags & V4L2_CTRL_FLAG_DISABLED) && query.type != V4L2_CTRL_TYPE_CTRL_CLASS)
        {
            V4l2Control control;
            control.id = query.id;
            control.name = reinterpret_cast<const char*>(query.name);
            control.type = query.type;
            control.min = query.minimum;
            control.max = query.maximum;
            control.step = query.step;
            control.inactive = query.flags & V4L2_CTRL_FLAG_INACTIVE;
            control.readOnly = query.flags & V4L2_CTRL_FLAG_READ_ONLY;
            control.hasValue = readControlValue(fd, query, control.value);

            if (query.type == V4L2_CTRL_TYPE_MENU || query.type == V4L2_CTRL_TYPE_INTEGER_MENU)
                control.menuItems = readMenuItems(fd, query);

            controls.push_back(std::move(control));
        }

        query.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }

    const bool success = errno == EINVAL;
    if (!success)
        std::cerr << "[V4L2] La enumeracion termino con un error.\n";

    ::close(fd);
    return success;
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

bool CameraUsb::capture(cv::Mat& frame)
{
    if (!open())
        return false;

    const bool success = read(frame);
    close();

    return success;
}
