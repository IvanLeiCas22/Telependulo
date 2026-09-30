#include "camera/camera.h"
#include "camera/cameraip.h"
#include "camera/camerausb.h"
#include "communication/httpserver.h"
#include "lighting/lighting.h"
#include "lighting/lightinggpio.h"
#include "web/pages.h"

#include <iostream>
#include <limits>
#include <linux/videodev2.h>
#include <mutex>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#include <string>
#include <utility>
#include <vector>

HttpResponse responderInicio(const HttpRequest& request)
{
    return HttpResponse::html(paginaInicio());
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

bool leerParametroQuery(const std::string& query, const std::string& key, std::string& value)
{
    std::size_t start = 0;

    while (start < query.size())
    {
        std::size_t end = query.find('&', start);
        if (end == std::string::npos)
            end = query.size();

        const std::string parameter = query.substr(start, end - start);
        const std::size_t separator = parameter.find('=');

        if (separator != std::string::npos && parameter.substr(0, separator) == key)
        {
            value = parameter.substr(separator + 1);
            return true;
        }

        start = end + 1;
    }

    return false;
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

    if (!leerParametroQuery(request.query, "id", idText) ||
        !leerParametroQuery(request.query, "value", valueText))
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

HttpResponse responderControlesCamara(CameraUsb& camera, std::mutex& cameraMutex)
{
    std::vector<V4l2Control> controls;
    {
        std::lock_guard<std::mutex> lock(cameraMutex);
        controls = camera.getV4l2Controls();
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

HttpResponse responderCaptura(Camera& camera, std::mutex& cameraMutex)
{
    cv::Mat frame;
    {
        std::lock_guard<std::mutex> lock(cameraMutex);
        if (!camera.capture(frame))
            return HttpResponse::text("No se pudo capturar la imagen.", 500);
    }

    std::vector<unsigned char> buffer;
    if (!cv::imencode(".png", frame, buffer))
        return HttpResponse::text("No se pudo codificar la imagen.", 500);

    std::string body(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    return {200, "image/png", std::move(body)};
}

bool capturarJpeg(Camera& camera, std::mutex& cameraMutex, std::vector<unsigned char>& jpeg)
{
    cv::Mat frame;
    {
        std::lock_guard<std::mutex> lock(cameraMutex);
        if (!camera.capture(frame))
            return false;
    }

    jpeg.clear();
    return cv::imencode(".jpg", frame, jpeg);
}

bool leerJpeg(CameraUsb& camera, std::mutex& cameraMutex, std::vector<unsigned char>& jpeg)
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

int main()
{
    UsbCameraConfig usbConfig;
    usbConfig.deviceIndex = 0;
    usbConfig.width = 1920;
    usbConfig.height = 1080;
    usbConfig.fps = 5;
    usbConfig.fourcc = cv::VideoWriter::fourcc('Y', 'U', 'Y', 'V');
    CameraUsb camera1(usbConfig);
    std::mutex camera1Mutex;

    IpCameraConfig ipConfig;
    ipConfig.url = "http://192.168.10.119:8080/camera1/photo.jpg";
    ipConfig.timeoutMs = 10000;
    CameraIp camera2(ipConfig);
    std::mutex camera2Mutex;

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

    server.get("/camera/1/controls", [&camera1, &camera1Mutex](const HttpRequest&)
    {
        return responderControlesCamara(camera1, camera1Mutex);
    });

    server.put("/camera/1/control", [&camera1, &camera1Mutex](const HttpRequest& request)
    {
        return responderCambioControlCamara(camera1, camera1Mutex, request);
    });

    server.get("/capture/1", [&camera1, &camera1Mutex](const HttpRequest&)      // Capturar y devolver la cámara USB
    {
        return responderCaptura(camera1, camera1Mutex);
    });
    server.get("/capture/2", [&camera2, &camera2Mutex](const HttpRequest&)      // Capturar y devolver la cámara IP
    {
        return responderCaptura(camera2, camera2Mutex);
    });
    server.stream("/stream/1",
                  [&camera1, &camera1Mutex]()
                  {
                      std::lock_guard<std::mutex> lock(camera1Mutex);
                      return camera1.open();
                  },
                  [&camera1, &camera1Mutex](std::vector<unsigned char>& jpeg)
                  {
                      return leerJpeg(camera1, camera1Mutex, jpeg);
                  },
                  [&camera1, &camera1Mutex]()
                  {
                      std::lock_guard<std::mutex> lock(camera1Mutex);
                      camera1.close();
                  });
    server.stream("/stream/2", [&camera2, &camera2Mutex](std::vector<unsigned char>& jpeg)
    {
        return capturarJpeg(camera2, camera2Mutex, jpeg);
    });

    if (!server.run())                                          // Arrancar el server
    {
        std::cerr << "El servidor HTTP se detuvo por un error.\n";
        return 1;
    }

    return 0;
}
