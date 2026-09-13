#include "cameraip.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <fcntl.h>
#include <iostream>
#include <netdb.h>
#include <opencv2/imgcodecs.hpp>
#include <poll.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>

namespace
{
constexpr std::size_t MAX_IMAGE_BYTES = 64 * 1024 * 1024;

struct ParsedUrl
{
    std::string host;
    std::string port = "80";
    std::string path = "/";
};

bool parseUrl(const std::string& url, ParsedUrl& parsed)
{
    constexpr const char* prefix = "http://";
    if (url.rfind(prefix, 0) != 0)
        return false;

    std::string remainder = url.substr(7);
    std::size_t pathStart = remainder.find('/');
    std::string authority = pathStart == std::string::npos ? remainder : remainder.substr(0, pathStart);
    parsed.path = pathStart == std::string::npos ? "/" : remainder.substr(pathStart);

    if (authority.empty())
        return false;

    std::size_t colon = authority.rfind(':');
    if (colon != std::string::npos)
    {
        parsed.host = authority.substr(0, colon);
        parsed.port = authority.substr(colon + 1);
    }
    else
    {
        parsed.host = authority;
    }

    if (parsed.host.empty() || parsed.port.empty())
        return false;

    try
    {
        int port = std::stoi(parsed.port);
        if (port < 1 || port > 65535)
            return false;
    }
    catch (...)
    {
        return false;
    }

    return true;
}

void setSocketTimeouts(int socket, int timeoutMs)
{
    timeval timeout{};
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

int connectSocket(const ParsedUrl& url, int timeoutMs)
{
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* addresses = nullptr;
    if (getaddrinfo(url.host.c_str(), url.port.c_str(), &hints, &addresses) != 0)
        return -1;

    int connectedSocket = -1;

    for (addrinfo* address = addresses; address != nullptr && connectedSocket < 0; address = address->ai_next)
    {
        int socketFd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (socketFd < 0)
            continue;

        int flags = fcntl(socketFd, F_GETFL, 0);
        if (flags < 0 || fcntl(socketFd, F_SETFL, flags | O_NONBLOCK) < 0)
        {
            close(socketFd);
            continue;
        }

        int result = connect(socketFd, address->ai_addr, address->ai_addrlen);
        if (result < 0 && errno == EINPROGRESS)
        {
            pollfd pollSocket{};
            pollSocket.fd = socketFd;
            pollSocket.events = POLLOUT;

            result = poll(&pollSocket, 1, timeoutMs);
            if (result > 0)
            {
                int socketError = 0;
                socklen_t socketErrorSize = sizeof(socketError);
                if (getsockopt(socketFd, SOL_SOCKET, SO_ERROR, &socketError, &socketErrorSize) < 0 || socketError != 0)
                    result = -1;
                else
                    result = 0;
            }
            else
            {
                result = -1;
            }
        }

        if (result == 0)
        {
            fcntl(socketFd, F_SETFL, flags);
            setSocketTimeouts(socketFd, timeoutMs);
            connectedSocket = socketFd;
        }
        else
        {
            close(socketFd);
        }
    }

    freeaddrinfo(addresses);
    return connectedSocket;
}

bool sendAll(int socket, const char* data, std::size_t size)
{
    std::size_t sentTotal = 0;

    while (sentTotal < size)
    {
        ssize_t sent = send(socket, data + sentTotal, size - sentTotal, MSG_NOSIGNAL);
        if (sent <= 0)
            return false;

        sentTotal += static_cast<std::size_t>(sent);
    }

    return true;
}

std::string lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char character)
    {
        return static_cast<char>(std::tolower(character));
    });
    return text;
}

bool extractHttpImage(const std::vector<unsigned char>& raw, std::vector<unsigned char>& image)
{
    static const std::string separator = "\r\n\r\n";
    auto headerEnd = std::search(raw.begin(), raw.end(), separator.begin(), separator.end());
    if (headerEnd == raw.end())
        return false;

    std::string headers(raw.begin(), headerEnd);
    std::istringstream stream(headers);

    std::string statusLine;
    if (!std::getline(stream, statusLine))
        return false;
    if (!statusLine.empty() && statusLine.back() == '\r')
        statusLine.pop_back();

    std::istringstream statusStream(statusLine);
    std::string httpVersion;
    int statusCode = 0;
    statusStream >> httpVersion >> statusCode;
    if (httpVersion.rfind("HTTP/", 0) != 0 || statusCode != 200)
        return false;

    std::size_t contentLength = 0;
    bool hasContentLength = false;
    bool chunked = false;

    std::string line;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        std::size_t colon = line.find(':');
        if (colon == std::string::npos)
            continue;

        std::string name = lower(line.substr(0, colon));
        std::string value = line.substr(colon + 1);
        std::size_t first = value.find_first_not_of(" \t");
        value = first == std::string::npos ? "" : value.substr(first);

        if (name == "content-length")
        {
            try
            {
                contentLength = static_cast<std::size_t>(std::stoull(value));
                hasContentLength = true;
            }
            catch (...)
            {
                return false;
            }
        }
        else if (name == "transfer-encoding" && lower(value).find("chunked") != std::string::npos)
        {
            chunked = true;
        }
    }

    if (chunked)
    {
        std::cerr << "[CameraIp] Transfer-Encoding: chunked todavia no esta soportado.\n";
        return false;
    }

    auto bodyStart = headerEnd + separator.size();
    image.assign(bodyStart, raw.end());

    if (hasContentLength && image.size() != contentLength)
        return false;

    return !image.empty();
}

bool getHttpImage(const std::string& url, int timeoutMs, std::vector<unsigned char>& image)
{
    ParsedUrl parsed;
    if (!parseUrl(url, parsed))
    {
        std::cerr << "[CameraIp] URL no valida o protocolo no soportado: " << url << "\n";
        return false;
    }

    int socket = connectSocket(parsed, timeoutMs);
    if (socket < 0)
    {
        std::cerr << "[CameraIp] No se pudo conectar a " << parsed.host << ':' << parsed.port << ".\n";
        return false;
    }

    std::string request =
        "GET " + parsed.path + " HTTP/1.1\r\n"
        "Host: " + parsed.host + "\r\n"
        "Accept: image/*\r\n"
        "Connection: close\r\n"
        "\r\n";

    if (!sendAll(socket, request.data(), request.size()))
    {
        close(socket);
        std::cerr << "[CameraIp] No se pudo enviar la peticion HTTP.\n";
        return false;
    }

    std::vector<unsigned char> raw;
    unsigned char buffer[8192];

    while (true)
    {
        ssize_t received = recv(socket, buffer, sizeof(buffer), 0);
        if (received == 0)
            break;

        if (received < 0)
        {
            close(socket);
            std::cerr << "[CameraIp] Error o timeout recibiendo la imagen.\n";
            return false;
        }

        raw.insert(raw.end(), buffer, buffer + received);
        if (raw.size() > MAX_IMAGE_BYTES + 65536)
        {
            close(socket);
            std::cerr << "[CameraIp] La respuesta HTTP es demasiado grande.\n";
            return false;
        }
    }

    close(socket);

    if (!extractHttpImage(raw, image))
    {
        std::cerr << "[CameraIp] La respuesta HTTP no es valida o no esta soportada.\n";
        return false;
    }

    if (image.size() > MAX_IMAGE_BYTES)
    {
        std::cerr << "[CameraIp] La imagen excede el tamano maximo permitido.\n";
        image.clear();
        return false;
    }

    return true;
}
}

CameraIp::CameraIp(const IpCameraConfig& config) : config_(config)
{
}

bool CameraIp::capture(cv::Mat& frame) const
{
    frame.release();

    std::vector<unsigned char> data;
    if (!getHttpImage(config_.url, config_.timeoutMs, data))
        return false;

    frame = cv::imdecode(data, cv::IMREAD_COLOR);
    if (frame.empty())
    {
        std::cerr << "[CameraIp] La respuesta no contiene una imagen valida.\n";
        return false;
    }

    return true;
}
