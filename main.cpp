#include "server/httpserver.h"
#include "web/pages.h"

#include <iostream>

HttpResponse responderInicio(const HttpRequest& request)
{
    return HttpResponse::html(paginaInicio());
}

HttpResponse responderConfiguracion(const HttpRequest& request)
{
    return HttpResponse::html(paginaConfiguracion());
}

int main()
{
    HttpServer server(8080);                                    // Crear el server en el puerto 8080
    server.get("/", responderInicio);                           // Enlazar la ruta / con la página de inicio
    server.get("/config", responderConfiguracion);              // Enlazar la ruta /config con la página de configuración

    if (!server.run())                                          // Arrancar el server
    {
        std::cerr << "El servidor HTTP se detuvo por un error.\n";
        return 1;
    }

    return 0;
}
