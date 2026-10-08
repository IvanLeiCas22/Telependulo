#include "httpserver.h"

#include <cerrno>                                       // Para indicadores de fallo del sistema
#include <iostream>                                     // Para dar salidas normales o de errores
#include <sstream>                                      // Para separar la primera linea de una petición http
#include <thread>                                       // Para atender cada cliente en un hilo independiente
#include <utility>                                      // Para move que copia algo de un lugar a otro sin conservar el original

// Headers del sistema linux
#include <netinet/in.h>                                 // En que puerto e interfaz escucha el servidor
#include <netinet/tcp.h>                                // Para limitar datos TCP pendientes del stream
#include <sys/socket.h>                                 // Funciones de sockets
#include <unistd.h>                                     // Para cerrar los sockets

namespace
{
constexpr std::size_t MAX_REQUEST_SIZE = 8192;
constexpr unsigned int STREAM_TCP_NOTSENT_LOWAT = 16 * 1024;

void configureStreamSocket(int socket)
{
#ifdef TCP_NOTSENT_LOWAT
    const unsigned int notSentLowat = STREAM_TCP_NOTSENT_LOWAT;

    if (setsockopt(socket, IPPROTO_TCP, TCP_NOTSENT_LOWAT,
                   &notSentLowat, sizeof(notSentLowat)) < 0)
    {
        std::cerr << "[HttpServer] No se pudo limitar la cola TCP del stream.\n";
    }
#else
    (void)socket;
    std::cerr << "[HttpServer] TCP_NOTSENT_LOWAT no esta disponible en este sistema.\n";
#endif
}

std::string statusText(int statusCode)
{
    switch (statusCode)
    {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 409: return "Conflict";
    case 405: return "Method Not Allowed";
    default: return "Internal Server Error";
    }
}
}

bool HttpRequest::queryParam(const std::string& key, std::string& value) const
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

HttpResponse HttpResponse::html(std::string body, int statusCode)
{
    return {statusCode, "text/html; charset=utf-8", std::move(body)};
}

HttpResponse HttpResponse::text(std::string body, int statusCode)
{
    return {statusCode, "text/plain; charset=utf-8", std::move(body)};
}

HttpServer::HttpServer(int port) : port_(port)
{
}

void HttpServer::get(const std::string& path, HttpHandler handler)
{
    getRoutes_[path] = std::move(handler);
}

void HttpServer::put(const std::string& path, HttpHandler handler)
{
    putRoutes_[path] = std::move(handler);
}

void HttpServer::stream(const std::string& path, HttpStreamSessionFactory createSession)
{
    HttpStreamRoute route;
    route.createSession = std::move(createSession);
    streamRoutes_[path] = std::move(route);
}

bool HttpServer::run()
{
    // Crear el socket TCP IPv4.
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0); // AF_INET: IPV4, SOCK_STREAM: TCP
    if (serverSocket < 0)
    {
        std::cerr << "No se pudo crear el socket del servidor.\n";
        return false;
    }

    int reuseAddress = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuseAddress, sizeof(reuseAddress));


    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;       // aceptar conexiones en cualquiera de las interfaces de red del equipo.
    address.sin_port = htons(port_);

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
    {
        std::cerr << "No se pudo usar el puerto " << port_ << ".\n";
        close(serverSocket);
        return false;
    }

    if (listen(serverSocket, 8) < 0)
    {
        std::cerr << "No se pudo iniciar la escucha HTTP.\n";
        close(serverSocket);
        return false;
    }

    std::cout << "Telependulo escuchando en http://0.0.0.0:" << port_ << "/\n";

    while (true)
    {
        int clientSocket = accept(serverSocket, nullptr, nullptr);          // Bloqueante
        if (clientSocket < 0)
        {
            if (errno == EINTR) continue;
            std::cerr << "Error aceptando una conexion.\n";
            close(serverSocket);
            return false;
        }

        std::thread([this, clientSocket]()
        {
            handleClient(clientSocket);
            close(clientSocket);
        }).detach();
    }
}

bool HttpServer::handleClient(int clientSocket)
{
    HttpRequest request;
    if (!receiveRequest(clientSocket, request))
        return sendResponse(clientSocket, HttpResponse::text("Bad Request", 400));

    if (request.method == "GET")
    {
        auto streamRoute = streamRoutes_.find(request.path);
        if (streamRoute != streamRoutes_.end())
            return sendStream(clientSocket, streamRoute->second);
    }

    return sendResponse(clientSocket, route(request));
}

bool HttpServer::receiveRequest(int clientSocket, HttpRequest& request)
{
    std::string data;
    char buffer[1024];

    while (data.find("\r\n\r\n") == std::string::npos)
    {
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) return false;

        data.append(buffer, static_cast<std::size_t>(bytesRead));
        if (data.size() > MAX_REQUEST_SIZE) return false;
    }

    std::istringstream firstLine(data.substr(0, data.find("\r\n")));
    std::string target;
    std::string httpVersion;
    firstLine >> request.method >> target >> httpVersion;

    const std::size_t queryStart = target.find('?');
    request.path = target.substr(0, queryStart);

    if (queryStart != std::string::npos)
        request.query = target.substr(queryStart + 1);

    return !request.method.empty() && !request.path.empty() && httpVersion.rfind("HTTP/", 0) == 0;
}

HttpResponse HttpServer::route(const HttpRequest& request) const
{
    if (request.method == "GET")
    {
        auto route = getRoutes_.find(request.path);

        if (route == getRoutes_.end())
            return HttpResponse::text("Not Found", 404);

        return route->second(request);
    }

    if (request.method == "PUT")
    {
        auto route = putRoutes_.find(request.path);

        if (route == putRoutes_.end())
            return HttpResponse::text("Not Found", 404);

        return route->second(request);
    }

    return HttpResponse::text("Method Not Allowed", 405);
}

bool HttpServer::sendResponse(int clientSocket, const HttpResponse& response)
{
    std::string header =
        "HTTP/1.1 " + std::to_string(response.statusCode) + " " + statusText(response.statusCode) + "\r\n"
        "Content-Type: " + response.contentType + "\r\n"
        "Content-Length: " + std::to_string(response.body.size()) + "\r\n"
        "Connection: close\r\n";

    for (const auto& extra : response.headers)
        header += extra.first + ": " + extra.second + "\r\n";

    header += "\r\n";
    return sendAll(clientSocket, header.data(), header.size()) && sendAll(clientSocket, response.body.data(), response.body.size());
}

bool HttpServer::sendStream(int clientSocket, const HttpStreamRoute& route)
{
    configureStreamSocket(clientSocket);

    HttpStreamSession session;
    if (!route.createSession || !route.createSession(session) || !session.frame)
        return sendResponse(clientSocket, HttpResponse::text("No se pudo iniciar el stream.", 500));

    auto stopSession = [&session]()
    {
        if (!session.stop)
            return;

        session.stop();
        session.stop = {};
    };

    static const std::string boundary = "telependulo-frame";
    std::string header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=" + boundary + "\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n"
        "\r\n";

    if (!sendAll(clientSocket, header.data(), header.size()))
    {
        stopSession();
        return false;
    }

    std::vector<unsigned char> frame;

    while (true)
    {
        frame.clear();

        if (!session.frame(frame) || frame.empty())
        {
            stopSession();
            return true;
        }

        std::string partHeader =
            "--" + boundary + "\r\n"
            "Content-Type: image/jpeg\r\n"
            "Content-Length: " + std::to_string(frame.size()) + "\r\n"
            "\r\n";

        if (!sendAll(clientSocket, partHeader.data(), partHeader.size()) ||
            !sendAll(clientSocket, reinterpret_cast<const char*>(frame.data()), frame.size()) ||
            !sendAll(clientSocket, "\r\n", 2))
        {
            stopSession();
            return false;
        }
    }
}

bool HttpServer::sendAll(int socket, const char* data, std::size_t size)
{
    std::size_t sentTotal = 0;

    while (sentTotal < size)
    {
        ssize_t sent = send(socket, data + sentTotal, size - sentTotal, MSG_NOSIGNAL);
        if (sent <= 0) return false;
        sentTotal += static_cast<std::size_t>(sent);
    }

    return true;
}
