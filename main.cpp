#include "camera/camera.h"
#include "camera/cameraip.h"
#include "camera/camerausb.h"
#include "communication/httpserver.h"
#include "lighting/lighting.h"
#include "lighting/lightinggpio.h"
#include "web/pages.h"

#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <linux/videodev2.h>
#include <memory>
#include <mutex>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#include <string>
#include <thread>
#include <utility>
#include <vector>

HttpResponse responderInicio(const HttpRequest& request)
{
    return HttpResponse::html(paginaCamaras());
}

HttpResponse responderConfiguracion(const HttpRequest& request)
{
    return HttpResponse::html(paginaConfiguracion());
}

HttpResponse responderEstadoLuz(Lighting& lighting, std::size_t channel)
{
    bool on = false;

    if (!lighting.get(channel, on))
        return HttpResponse::text("No se pudo leer la luz.", 500);

    return HttpResponse::text(on ? "on" : "off");
}

HttpResponse responderCambioLuz(Lighting& lighting, std::size_t channel, bool on)
{
    if (!lighting.set(channel, on))
        return HttpResponse::text("No se pudo cambiar la luz.", 500);

    return HttpResponse::text("ok");
}

HttpResponse responderTodasLasLuces(Lighting& lighting, bool on)
{
    if (!lighting.setAll(on))
        return HttpResponse::text("No se pudieron cambiar las luces.", 500);

    return HttpResponse::text("ok");
}

std::string escaparJson(const std::string& text)
{
    std::string result;

    for (char c : text)
    {
        if (c == '"' || c == '\\') result += '\\';
        if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else if (c == '\t') result += "\\t";
        else result += c;
    }

    return result;
}

bool convertirIdControl(const std::string& text, unsigned int& value)
{
    try
    {
        std::size_t parsed = 0;
        const unsigned long number = std::stoul(text, &parsed);

        if (parsed != text.size() || number > std::numeric_limits<unsigned int>::max())
            return false;

        value = static_cast<unsigned int>(number);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool convertirValorControl(const std::string& text, int& value)
{
    try
    {
        std::size_t parsed = 0;
        const long number = std::stol(text, &parsed);

        if (parsed != text.size() ||
            number < std::numeric_limits<int>::min() ||
            number > std::numeric_limits<int>::max())
        {
            return false;
        }

        value = static_cast<int>(number);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool convertirFps(const std::string& text, double& value)
{
    try
    {
        std::size_t parsed = 0;
        value = std::stod(text, &parsed);

        return parsed == text.size() && std::isfinite(value) && value > 0;
    }
    catch (...)
    {
        return false;
    }
}

const char* tipoControlV4l2(unsigned int type)
{
    switch (type)
    {
    case V4L2_CTRL_TYPE_INTEGER: return "integer";
    case V4L2_CTRL_TYPE_BOOLEAN: return "boolean";
    case V4L2_CTRL_TYPE_MENU: return "menu";
    case V4L2_CTRL_TYPE_INTEGER_MENU: return "integer-menu";
    default: return nullptr;
    }
}

HttpResponse responderCambioControlCamara(CameraUsb& camera, std::mutex& cameraMutex, const HttpRequest& request)
{
    std::string idText;
    std::string valueText;

    if (!request.queryParam("id", idText) ||
        !request.queryParam("value", valueText))
    {
        return HttpResponse::text("Faltan parametros id o value.", 400);
    }

    unsigned int id = 0;
    int value = 0;

    if (!convertirIdControl(idText, id) || !convertirValorControl(valueText, value))
        return HttpResponse::text("Parametros id o value invalidos.", 400);

    std::lock_guard<std::mutex> lock(cameraMutex);

    if (!camera.setV4l2Control(id, value))
        return HttpResponse::text("No se pudo cambiar el control de la camara.", 500);

    return HttpResponse::text("ok");
}

HttpResponse responderModosCamara(CameraUsb& camera, std::mutex& cameraMutex)
{
    std::vector<CameraMode> modes;
    {
        std::lock_guard<std::mutex> lock(cameraMutex);

        if (!camera.getAvailableModes(modes))
            return HttpResponse::text("No se pudieron consultar los modos de la cámara.", 500);
    }

    std::string json = "[";
    for (std::size_t i = 0; i < modes.size(); ++i)
    {
        if (i > 0) json += ",";

        json += "{";
        json += "\"width\":" + std::to_string(modes[i].width);
        json += ",\"height\":" + std::to_string(modes[i].height);
        json += ",\"fps\":" + std::to_string(modes[i].fps);
        json += "}";
    }
    json += "]";

    return {200, "application/json; charset=utf-8", std::move(json)};
}

HttpResponse responderCambioModoCamara(CameraUsb& camera, std::mutex& cameraMutex, const HttpRequest& request)
{
    std::string widthText;
    std::string heightText;
    std::string fpsText;

    if (!request.queryParam("width", widthText) ||
        !request.queryParam("height", heightText) ||
        !request.queryParam("fps", fpsText))
    {
        return HttpResponse::text("Faltan parametros width, height o fps.", 400);
    }

    int width = 0;
    int height = 0;
    double fps = 0;

    if (!convertirValorControl(widthText, width) ||
        !convertirValorControl(heightText, height) ||
        !convertirFps(fpsText, fps))
    {
        return HttpResponse::text("Parametros de modo invalidos.", 400);
    }

    std::lock_guard<std::mutex> lock(cameraMutex);

    if (!camera.setMode(width, height, fps))
        return HttpResponse::text("No se pudo cambiar el modo de la camara.", 500);

    return HttpResponse::text("ok");
}

HttpResponse responderControlesCamara(CameraUsb& camera, std::mutex& cameraMutex)
{
    std::vector<V4l2Control> controls;
    {
        std::lock_guard<std::mutex> lock(cameraMutex);

        if (!camera.getV4l2Controls(controls))
            return HttpResponse::text("No se pudieron consultar los controles de la cámara.", 500);
    }

    std::string json = "[";
    bool firstControl = true;

    for (const auto& control : controls)
    {
        const char* type = tipoControlV4l2(control.type);
        if (!type || !control.hasValue || control.readOnly)
            continue;

        if (!firstControl) json += ",";
        firstControl = false;

        json += "{";
        json += "\"id\":" + std::to_string(control.id);
        json += ",\"name\":\"" + escaparJson(control.name) + "\"";
        json += ",\"type\":\"" + std::string(type) + "\"";
        json += ",\"value\":" + std::to_string(control.value);
        json += ",\"inactive\":" + std::string(control.inactive ? "true" : "false");

        if (control.type == V4L2_CTRL_TYPE_INTEGER)
        {
            json += ",\"min\":" + std::to_string(control.min);
            json += ",\"max\":" + std::to_string(control.max);
            json += ",\"step\":" + std::to_string(control.step);
        }

        if (control.type == V4L2_CTRL_TYPE_MENU || control.type == V4L2_CTRL_TYPE_INTEGER_MENU)
        {
            json += ",\"options\":[";
            for (std::size_t i = 0; i < control.menuItems.size(); ++i)
            {
                if (i > 0) json += ",";
                json += "{\"value\":" + std::to_string(control.menuItems[i].value);
                json += ",\"name\":\"" + escaparJson(control.menuItems[i].name) + "\"}";
            }
            json += "]";
        }

        json += "}";
    }

    json += "]";
    return {200, "application/json; charset=utf-8", std::move(json)};
}

bool guardarCaptura(const std::vector<unsigned char>& buffer, int numero)
{
    namespace fs = std::filesystem;

    const fs::path imagesDir = fs::path(TELEPENDULO_PROJECT_ROOT) / "images";
    const fs::path finalPath = imagesDir / ("camera" + std::to_string(numero) + ".png");
    const fs::path tempPath = imagesDir / ("camera" + std::to_string(numero) + ".tmp");

    std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
    if (!file)
        return false;

    file.write(reinterpret_cast<const char*>(buffer.data()),
               static_cast<std::streamsize>(buffer.size()));
    file.close();

    if (!file)
    {
        std::error_code error;
        fs::remove(tempPath, error);
        return false;
    }

    std::error_code error;
    fs::rename(tempPath, finalPath, error);

    if (error)
    {
        fs::remove(tempPath, error);
        return false;
    }

    return true;
}

HttpResponse responderCaptura(Camera& camera, std::mutex& cameraMutex, int numero)
{
    cv::Mat frame;
    std::vector<unsigned char> buffer;

    {
        std::lock_guard<std::mutex> lock(cameraMutex);

        if (!camera.capture(frame))
            return HttpResponse::text("No se pudo capturar la imagen.", 500);

        if (!cv::imencode(".png", frame, buffer))
            return HttpResponse::text("No se pudo codificar la imagen.", 500);

        if (!guardarCaptura(buffer, numero))
            return HttpResponse::text("No se pudo guardar la captura.", 500);
    }

    std::string body(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    return {200, "image/png", std::move(body)};
}

bool leerJpeg(Camera& camera, std::mutex& cameraMutex, std::vector<unsigned char>& jpeg)
{
    cv::Mat frame;
    {
        std::lock_guard<std::mutex> lock(cameraMutex);
        if (!camera.read(frame))
            return false;
    }

    jpeg.clear();
    return cv::imencode(".jpg", frame, jpeg);
}

struct CameraStreamState
{
    std::mutex lifecycleMutex;
    std::mutex frameMutex;
    std::condition_variable frameReady;
    std::thread producer;

    std::vector<unsigned char> latestJpeg;
    std::uint64_t version = 0;
    std::size_t clients = 0;

    std::atomic<bool> running{false};
    std::atomic<bool> failed{false};
};

void producirStreamCamara(Camera& camera, std::mutex& cameraMutex,
                          const std::shared_ptr<CameraStreamState>& state)
{
    while (state->running.load())
    {
        std::vector<unsigned char> jpeg;

        if (!leerJpeg(camera, cameraMutex, jpeg))
        {
            state->failed.store(true);
            state->running.store(false);
            state->frameReady.notify_all();
            return;
        }

        if (!state->running.load())
            break;

        {
            std::lock_guard<std::mutex> lock(state->frameMutex);
            state->latestJpeg = std::move(jpeg);
            ++state->version;
        }

        state->frameReady.notify_all();
    }

    state->frameReady.notify_all();
}

void cerrarSesionStreamCamara(Camera& camera, std::mutex& cameraMutex,
                              const std::shared_ptr<CameraStreamState>& state)
{
    std::lock_guard<std::mutex> lifecycleLock(state->lifecycleMutex);

    if (state->clients == 0)
        return;

    --state->clients;

    if (state->clients > 0)
        return;

    state->running.store(false);
    state->frameReady.notify_all();

    if (state->producer.joinable())
        state->producer.join();

    {
        std::lock_guard<std::mutex> lock(cameraMutex);
        camera.close();
    }

    {
        std::lock_guard<std::mutex> lock(state->frameMutex);
        state->latestJpeg.clear();
        state->version = 0;
    }

    state->failed.store(false);
}

bool crearSesionStreamCamara(Camera& camera, std::mutex& cameraMutex,
                             const std::shared_ptr<CameraStreamState>& state,
                             HttpStreamSession& session)
{
    std::lock_guard<std::mutex> lifecycleLock(state->lifecycleMutex);

    if (state->clients == 0)
    {
        {
            std::lock_guard<std::mutex> lock(cameraMutex);
            if (!camera.open())
                return false;
        }

        {
            std::lock_guard<std::mutex> lock(state->frameMutex);
            state->latestJpeg.clear();
            state->version = 0;
        }

        state->failed.store(false);
        state->running.store(true);

        state->producer = std::thread([&camera, &cameraMutex, state]()
        {
            producirStreamCamara(camera, cameraMutex, state);
        });
    }
    else if (!state->running.load() || state->failed.load())
    {
        return false;
    }

    ++state->clients;

    session.frame = [state, lastVersion = std::uint64_t{0}]
                    (std::vector<unsigned char>& jpeg) mutable
    {
        std::unique_lock<std::mutex> lock(state->frameMutex);

        state->frameReady.wait(lock, [&]()
        {
            return state->version != lastVersion ||
                   state->failed.load() ||
                   !state->running.load();
        });

        if (state->failed.load() || !state->running.load())
            return false;

        jpeg = state->latestJpeg;
        lastVersion = state->version;
        return !jpeg.empty();
    };

    session.stop = [&camera, &cameraMutex, state]()
    {
        cerrarSesionStreamCamara(camera, cameraMutex, state);
    };

    return true;
}

struct CameraSetup
{
    std::unique_ptr<Camera> camera;
    CameraUsb* usb = nullptr;
};

CameraSetup crearCamaraUsb(const UsbCameraConfig& config)
{
    auto camera = std::make_unique<CameraUsb>(config);
    CameraUsb* usb = camera.get();

    return {std::move(camera), usb};
}

CameraSetup crearCamaraIp(const IpCameraConfig& config)
{
    return {std::make_unique<CameraIp>(config), nullptr};
}

void registrarCamara(HttpServer& server, int numero, Camera& camera, std::mutex& cameraMutex)
{
    const std::string id = std::to_string(numero);
    auto streamState = std::make_shared<CameraStreamState>();

    server.get("/capture/" + id, [&camera, &cameraMutex, numero](const HttpRequest&)
    {
        return responderCaptura(camera, cameraMutex, numero);
    });

    server.stream("/stream/" + id,
                  [&camera, &cameraMutex, streamState](HttpStreamSession& session)
                  {
                      return crearSesionStreamCamara(camera, cameraMutex, streamState, session);
                  });
}

void registrarConfiguracionUsb(HttpServer& server, int numero, CameraUsb& camera,
                               std::mutex& cameraMutex)
{
    const std::string id = std::to_string(numero);
    const std::string base = "/camera/" + id;

    server.get(base + "/controls", [&camera, &cameraMutex](const HttpRequest&)
    {
        return responderControlesCamara(camera, cameraMutex);
    });

    server.get(base + "/modes", [&camera, &cameraMutex](const HttpRequest&)
    {
        return responderModosCamara(camera, cameraMutex);
    });

    server.put(base + "/control", [&camera, &cameraMutex](const HttpRequest& request)
    {
        return responderCambioControlCamara(camera, cameraMutex, request);
    });

    server.put(base + "/mode", [&camera, &cameraMutex](const HttpRequest& request)
    {
        return responderCambioModoCamara(camera, cameraMutex, request);
    });
}

int main()
{
    UsbCameraConfig usbConfig;
    usbConfig.deviceIndex = 0;
    usbConfig.width = 1920;
    usbConfig.height = 1080;
    usbConfig.fps = 5;
    usbConfig.fourcc = cv::VideoWriter::fourcc('Y', 'U', 'Y', 'V');
    CameraSetup camera1 = crearCamaraUsb(usbConfig);
    std::mutex camera1Mutex;

    UsbCameraConfig usbConfig2 = usbConfig;
    usbConfig2.deviceIndex = 2;
    CameraSetup camera2 = crearCamaraUsb(usbConfig2);
    std::mutex camera2Mutex;

    namespace fs = std::filesystem;
    const fs::path imagesDir = fs::path(TELEPENDULO_PROJECT_ROOT) / "images";
    std::error_code imagesError;

    fs::create_directories(imagesDir, imagesError);
    if (imagesError)
    {
        std::cerr << "No se pudo crear la carpeta de imagenes: "
                  << imagesDir << "\n";
        return 1;
    }

    GpioLightingConfig lightingConfig;
    lightingConfig.chipPath = "/dev/gpiochip0";
    lightingConfig.offsets = {22, 27};
    lightingConfig.activeLow = false;
    LightingGpio lighting(lightingConfig);

    HttpServer server(8080);                                    // Crear el server en el puerto 8080
    server.get("/", responderInicio);                           // Enlazar la ruta / con la página de inicio
    server.get("/config", responderConfiguracion);              // Enlazar la ruta /config con la página de configuración
    server.get("/lighting/1", [&lighting](const HttpRequest&)
               {
                   return responderEstadoLuz(lighting, 0);
               });

    server.get("/lighting/2", [&lighting](const HttpRequest&)
               {
                   return responderEstadoLuz(lighting, 1);
               });

    server.put("/lighting/1/on", [&lighting](const HttpRequest&)
               {
                   return responderCambioLuz(lighting, 0, true);
               });

    server.put("/lighting/1/off", [&lighting](const HttpRequest&)
               {
                   return responderCambioLuz(lighting, 0, false);
               });

    server.put("/lighting/2/on", [&lighting](const HttpRequest&)
               {
                   return responderCambioLuz(lighting, 1, true);
               });

    server.put("/lighting/2/off", [&lighting](const HttpRequest&)
               {
                   return responderCambioLuz(lighting, 1, false);
               });

    server.put("/lighting/all/on", [&lighting](const HttpRequest&)
               {
                   return responderTodasLasLuces(lighting, true);
               });

    server.put("/lighting/all/off", [&lighting](const HttpRequest&)
               {
                   return responderTodasLasLuces(lighting, false);
               });

    registrarCamara(server, 1, *camera1.camera, camera1Mutex);
    registrarCamara(server, 2, *camera2.camera, camera2Mutex);

    if (camera1.usb)
        registrarConfiguracionUsb(server, 1, *camera1.usb, camera1Mutex);

    if (camera2.usb)
        registrarConfiguracionUsb(server, 2, *camera2.usb, camera2Mutex);

    if (!server.run())                                          // Arrancar el server
    {
        std::cerr << "El servidor HTTP se detuvo por un error.\n";
        return 1;
    }

    return 0;
}
