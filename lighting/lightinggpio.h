#ifndef LIGHTINGGPIO_H
#define LIGHTINGGPIO_H

#include "lighting.h"

#include <gpiod.h>
#include <mutex>
#include <string>
#include <vector>

struct GpioLightingConfig
{
    std::string chipPath = "/dev/gpiochip0";
    std::vector<unsigned int> offsets;
    bool activeLow = false;
};

class LightingGpio : public Lighting
{
public:
    explicit LightingGpio(const GpioLightingConfig& config);
    ~LightingGpio() override;

    bool set(std::size_t channel, bool on) override;
    bool get(std::size_t channel, bool& on) const override;
    bool setAll(bool on) override;

private:
    GpioLightingConfig config_;

    gpiod_chip* chip_ = nullptr;
    gpiod_line_request* request_ = nullptr;

    mutable std::mutex mutex_;
};

#endif // LIGHTINGGPIO_H