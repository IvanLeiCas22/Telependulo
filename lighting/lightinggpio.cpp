#include "lightinggpio.h"

#include <iostream>
#include <vector>

LightingGpio::LightingGpio(const GpioLightingConfig& config)
    : config_(config)
{
    if (config_.offsets.empty())
    {
        std::cerr << "[LightingGpio] No se configuraron canales GPIO.\n";
        return;
    }

    chip_ = gpiod_chip_open(config_.chipPath.c_str());

    if (!chip_)
    {
        std::cerr << "[LightingGpio] No se pudo abrir "
                  << config_.chipPath << ".\n";
        return;
    }

    gpiod_line_settings* settings = gpiod_line_settings_new();
    gpiod_line_config* lineConfig = gpiod_line_config_new();
    gpiod_request_config* requestConfig = gpiod_request_config_new();

    if (!settings || !lineConfig || !requestConfig)
    {
        std::cerr << "[LightingGpio] No se pudo crear la configuración GPIO.\n";

        if (requestConfig)
            gpiod_request_config_free(requestConfig);

        if (lineConfig)
            gpiod_line_config_free(lineConfig);

        if (settings)
            gpiod_line_settings_free(settings);

        gpiod_chip_close(chip_);
        chip_ = nullptr;

        return;
    }

    if (gpiod_line_settings_set_direction(
            settings,
            GPIOD_LINE_DIRECTION_OUTPUT) < 0)
    {
        std::cerr << "[LightingGpio] No se pudo configurar GPIO como salida.\n";

        gpiod_request_config_free(requestConfig);
        gpiod_line_config_free(lineConfig);
        gpiod_line_settings_free(settings);

        gpiod_chip_close(chip_);
        chip_ = nullptr;

        return;
    }

    // La aplicación siempre trabaja con ON/OFF lógico.
    // libgpiod se ocupa de invertir físicamente el nivel si activeLow = true.
    gpiod_line_settings_set_active_low(
        settings,
        config_.activeLow);

    // Las luces arrancan apagadas.
    if (gpiod_line_settings_set_output_value(
            settings,
            GPIOD_LINE_VALUE_INACTIVE) < 0)
    {
        std::cerr << "[LightingGpio] No se pudo establecer el estado inicial.\n";

        gpiod_request_config_free(requestConfig);
        gpiod_line_config_free(lineConfig);
        gpiod_line_settings_free(settings);

        gpiod_chip_close(chip_);
        chip_ = nullptr;

        return;
    }

    if (gpiod_line_config_add_line_settings(
            lineConfig,
            config_.offsets.data(),
            config_.offsets.size(),
            settings) < 0)
    {
        std::cerr << "[LightingGpio] No se pudieron configurar las líneas GPIO.\n";

        gpiod_request_config_free(requestConfig);
        gpiod_line_config_free(lineConfig);
        gpiod_line_settings_free(settings);

        gpiod_chip_close(chip_);
        chip_ = nullptr;

        return;
    }

    gpiod_request_config_set_consumer(
        requestConfig,
        "telependulo-lighting");

    request_ = gpiod_chip_request_lines(
        chip_,
        requestConfig,
        lineConfig);

    // Estos objetos solo eran necesarios para crear request_.
    gpiod_request_config_free(requestConfig);
    gpiod_line_config_free(lineConfig);
    gpiod_line_settings_free(settings);

    if (!request_)
    {
        std::cerr << "[LightingGpio] No se pudieron solicitar las líneas GPIO.\n";

        gpiod_chip_close(chip_);
        chip_ = nullptr;

        return;
    }

    std::cout << "[LightingGpio] Iluminación inicializada con "
              << config_.offsets.size()
              << " canales.\n";
}

LightingGpio::~LightingGpio()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (request_)
    {
        // Intentar dejar todas las luces apagadas antes de liberar GPIO.
        std::vector<gpiod_line_value> values(
            config_.offsets.size(),
            GPIOD_LINE_VALUE_INACTIVE);

        gpiod_line_request_set_values(
            request_,
            values.data());

        gpiod_line_request_release(request_);
        request_ = nullptr;
    }

    if (chip_)
    {
        gpiod_chip_close(chip_);
        chip_ = nullptr;
    }
}

bool LightingGpio::set(std::size_t channel, bool on)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!request_ || channel >= config_.offsets.size())
        return false;

    const gpiod_line_value value =
        on
            ? GPIOD_LINE_VALUE_ACTIVE
            : GPIOD_LINE_VALUE_INACTIVE;

    if (gpiod_line_request_set_value(
            request_,
            config_.offsets[channel],
            value) < 0)
    {
        std::cerr << "[LightingGpio] No se pudo cambiar el canal "
                  << channel << ".\n";

        return false;
    }

    return true;
}

bool LightingGpio::get(std::size_t channel, bool& on) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!request_ || channel >= config_.offsets.size())
        return false;

    const gpiod_line_value value =
        gpiod_line_request_get_value(
            request_,
            config_.offsets[channel]);

    if (value == GPIOD_LINE_VALUE_ERROR)
        return false;

    on = (value == GPIOD_LINE_VALUE_ACTIVE);

    return true;
}

bool LightingGpio::setAll(bool on)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!request_)
        return false;

    const gpiod_line_value value =
        on
            ? GPIOD_LINE_VALUE_ACTIVE
            : GPIOD_LINE_VALUE_INACTIVE;

    std::vector<gpiod_line_value> values(
        config_.offsets.size(),
        value);

    if (gpiod_line_request_set_values(
            request_,
            values.data()) < 0)
    {
        std::cerr << "[LightingGpio] No se pudieron cambiar todos los canales.\n";
        return false;
    }

    return true;
}