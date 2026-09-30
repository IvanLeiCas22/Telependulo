#include "camera/camera.h"
#include "camera/cameraip.h"
#include "camera/camerausb.h"
#include "communication/httpserver.h"
#include "lighting/lighting.h"
#include "lighting/lightinggpio.h"
#include "web/pages.h"

#include <iostream>
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
    camera1.printV4l2Controls();
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
