#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <functional>                                   // Permite guardar funciones callback
#include <unordered_map>                                // Para la relación ruta web -> función que la atiende

#include <string>                                       // Uso de string
#include <cstddef>                                      // Para función size
#include <vector>
#include <utility>                                       // Para los JPEG del stream

struct HttpRequest
{
    std::string method;
    std::string path;
    std::string query;

    bool queryParam(const std::string& key, std::string& value) const;
};

struct HttpResponse
{
    int statusCode = 200;
    std::string contentType = "text/plain; charset=utf-8";
    std::string body;
    // Cabeceras adicionales; nombres y valores deben ser seguros para HTTP.
    std::vector<std::pair<std::string, std::string>> headers;

    HttpResponse() = default;
    HttpResponse(int status, std::string type, std::string responseBody,
                 std::vector<std::pair<std::string, std::string>> extraHeaders = {})
        : statusCode(status), contentType(std::move(type)),
          body(std::move(responseBody)), headers(std::move(extraHeaders)) {}

    static HttpResponse html(std::string body, int statusCode = 200);
    static HttpResponse text(std::string body, int statusCode = 200);
};

using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;
using HttpStreamHandler = std::function<bool(std::vector<unsigned char>&)>;
using HttpStreamStopHandler = std::function<void()>;

struct HttpStreamSession
{
    HttpStreamHandler frame;
    HttpStreamStopHandler stop;
};

using HttpStreamSessionFactory = std::function<bool(HttpStreamSession&)>;

struct HttpStreamRoute
{
    HttpStreamSessionFactory createSession;
};

class HttpServer
{
public:
    explicit HttpServer(int port = 8080);

    void get(const std::string& path, HttpHandler handler);                       // Registrar una ruta GET y decir qué callback debe atenderla
    void put(const std::string& path, HttpHandler handler);
    void stream(const std::string& path, HttpStreamSessionFactory createSession); // Registrar un stream MJPEG
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
