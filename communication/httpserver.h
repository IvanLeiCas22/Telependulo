#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <functional>                                   // Permite guardar una función que reciba const HttpRequest& y devuelva HttpResponse
#include <unordered_map>                                // Para la relación ruta web -> función que la atiende

#include <string>                                       // Uso de string
#include <cstddef>                                      // Para función size
#include <vector>                                       // Para los frames JPEG del stream

struct HttpRequest
{
    std::string method;
    std::string path;
};

struct HttpResponse
{
    int statusCode = 200;
    std::string contentType = "text/plain; charset=utf-8";
    std::string body;

    static HttpResponse html(std::string body, int statusCode = 200);
    static HttpResponse text(std::string body, int statusCode = 200);
};

using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;
using HttpStreamHandler = std::function<bool(std::vector<unsigned char>&)>;
using HttpStreamStartHandler = std::function<bool()>;
using HttpStreamStopHandler = std::function<void()>;

struct HttpStreamRoute
{
    HttpStreamStartHandler start;
    HttpStreamHandler frame;
    HttpStreamStopHandler stop;
};

class HttpServer
{
public:
    explicit HttpServer(int port = 8080);

    void get(const std::string& path, HttpHandler handler);                       // Registrar una ruta GET y decir qué callback debe atenderla
    void put(const std::string& path, HttpHandler handler);
    void stream(const std::string& path, HttpStreamHandler handler);               // Registrar un stream MJPEG
    void stream(const std::string& path, HttpStreamStartHandler start,
                HttpStreamHandler handler, HttpStreamStopHandler stop);
    bool run();

private:
    bool handleClient(int clientSocket);
    bool receiveRequest(int clientSocket, HttpRequest& request);
    bool sendResponse(int clientSocket, const HttpResponse& response);
    bool sendStream(int clientSocket, const HttpStreamRoute& route);
    bool sendAll(int socket, const char* data, std::size_t size);
    HttpResponse route(const HttpRequest& request) const;

    int port_;                                                                    // Instancia de la clase, conectada a port
    std::unordered_map<std::string, HttpHandler> getRoutes_;
    std::unordered_map<std::string, HttpHandler> putRoutes_;
    std::unordered_map<std::string, HttpStreamRoute> streamRoutes_;
};

#endif // HTTPSERVER_H
