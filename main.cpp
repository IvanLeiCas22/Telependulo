#include "server/httpserver.h"

#include <iostream>

HttpResponse paginaPrincipal(const HttpRequest& request)
{
    return HttpResponse::html(R"HTML(<!DOCTYPE html>
        <html lang="es">
        <head>
            <meta charset="UTF-8">
            <meta name="viewport" content="width=device-width, initial-scale=1.0">
            <title>Telepéndulo</title>
        </head>
        <body>
            <h1>Telepéndulo</h1>
        </body>
        </html>
        )HTML"
    );
}

int main()
{
    HttpServer server(8080);                    //Crear el server en el puerto 8080
    server.get("/", paginaPrincipal);           //Enlazar la ruta / con la función paginaPrincipal

    if (!server.run())                          //Arrancar el server
    {
        std::cerr << "El servidor HTTP se detuvo por un error.\n";
        return 1;
    }

    return 0;
}
