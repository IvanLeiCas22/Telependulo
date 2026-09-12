#include "pages.h"

namespace
{
std::string crearPagina(const std::string& titulo)
{
    return R"HTML(<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Telepéndulo</title>
    <style>
        * { box-sizing: border-box; }
        body { margin: 0; font-family: Arial, sans-serif; background: #f4f4f4; color: #222; }
        .hmi { display: flex; min-height: 100vh; }
        .sidebar { width: 220px; padding: 24px 16px; background: #20252b; }
        .sidebar h2 { margin: 0 0 24px; color: white; }
        .sidebar a { display: block; margin-bottom: 10px; padding: 12px 14px; border-radius: 6px; color: white; text-decoration: none; background: #30363d; }
        .sidebar a:hover { background: #3d444d; }
        .content { flex: 1; padding: 32px; }
        .content h1 { margin-top: 0; }
    </style>
</head>
<body>
    <div class="hmi">
        <nav class="sidebar">
            <h2>Telepéndulo</h2>
            <a href="/">Inicio</a>
            <a href="/config">Configuración</a>
        </nav>
        <main class="content">
            <h1>)HTML" + titulo + R"HTML(</h1>
        </main>
    </div>
</body>
</html>
)HTML";
}
}

std::string paginaInicio()
{
    return crearPagina("Inicio");
}

std::string paginaConfiguracion()
{
    return crearPagina("Configuración");
}
