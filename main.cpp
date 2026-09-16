#include "camera/camera.h"
#include "camera/cameraip.h"
#include "camera/camerausb.h"
#include "communication/httpserver.h"
#include "web/pages.h"

#include <iostream>
#include <mutex>
#include <opencv2/imgcodecs.hpp>
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

int main()
{
    UsbCameraConfig usbConfig;
    usbConfig.deviceIndex = 0;
    usbConfig.width = 1920;
    usbConfig.height = 1080;
    usbConfig.fps = 5;
    CameraUsb camera1(usbConfig);
    std::mutex camera1Mutex;

    IpCameraConfig ipConfig;
    ipConfig.url = "http://192.168.10.119:8080/camera1/photo.jpg";
    ipConfig.timeoutMs = 10000;
    CameraIp camera2(ipConfig);
    std::mutex camera2Mutex;

    HttpServer server(8080);                                    // Crear el server en el puerto 8080
    server.get("/", responderInicio);                           // Enlazar la ruta / con la página de inicio
    server.get("/config", responderConfiguracion);              // Enlazar la ruta /config con la página de configuración
    server.get("/capture/1", [&camera1, &camera1Mutex](const HttpRequest&)      // Capturar y devolver la cámara USB
    {
        return responderCaptura(camera1, camera1Mutex);
    });
    server.get("/capture/2", [&camera2, &camera2Mutex](const HttpRequest&)      // Capturar y devolver la cámara IP
    {
        return responderCaptura(camera2, camera2Mutex);
    });
    server.stream("/stream/1", [&camera1, &camera1Mutex](std::vector<unsigned char>& jpeg)
    {
        return capturarJpeg(camera1, camera1Mutex, jpeg);
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
