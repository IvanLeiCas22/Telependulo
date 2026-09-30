#include "pages.h"

namespace
{

std::string crearPagina(
    const std::string& titulo,
    const std::string& contenido = "")
{
    return R"HTML(<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Telepéndulo</title>

    <style>
        * {
            box-sizing: border-box;
        }

        body {
            margin: 0;
            font-family: Arial, sans-serif;
            background: #f4f4f4;
            color: #222;
        }

        .hmi {
            display: flex;
            min-height: 100vh;
        }

        .sidebar {
            width: 220px;
            padding: 24px 16px;
            background: #20252b;
        }

        .sidebar h2 {
            margin: 0 0 24px;
            color: white;
        }

        .sidebar a {
            display: block;
            margin-bottom: 10px;
            padding: 12px 14px;
            border-radius: 6px;
            color: white;
            text-decoration: none;
            background: #30363d;
        }

        .sidebar a:hover {
            background: #3d444d;
        }

        .content {
            flex: 1;
            padding: 32px;
        }

        .content h1 {
            margin-top: 0;
        }

        .panel {
            max-width: 650px;
            padding: 24px;
            border-radius: 10px;
            background: white;
            box-shadow: 0 2px 8px rgba(0, 0, 0, 0.08);
        }

        .light-row {
            display: flex;
            align-items: center;
            justify-content: space-between;

            padding: 18px 0;
            border-bottom: 1px solid #ddd;
        }

        .light-row:last-of-type {
            border-bottom: none;
        }

        .light-name {
            font-size: 18px;
            font-weight: bold;
        }

        .estado {
            margin-top: 5px;
            font-size: 14px;
        }

        .estado.encendida {
            color: #16803a;
        }

        .estado.apagada {
            color: #666;
        }

        .estado.error {
            color: #b42318;
        }

        .controls {
            display: flex;
            gap: 8px;
        }

        button {
            padding: 9px 14px;
            border: none;
            border-radius: 6px;
            cursor: pointer;
            font-size: 14px;
        }

        button.on {
            background: #16803a;
            color: white;
        }

        button.off {
            background: #d6d6d6;
            color: #222;
        }

        .all-controls {
            display: flex;
            gap: 8px;
            margin-top: 24px;
        }
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

        )HTML" + contenido + R"HTML(

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
    return crearPagina(
        "Configuración",

        R"HTML(

<section class="panel">

    <h2>Iluminación</h2>

    <div class="light-row">

        <div>
            <div class="light-name">Luz 1</div>

            <div id="estado-1" class="estado">
                Consultando...
            </div>
        </div>

        <button id="boton-1"
                onclick="alternarLuz(1)">
            ...
        </button>

    </div>


    <div class="light-row">

        <div>
            <div class="light-name">Luz 2</div>

            <div id="estado-2" class="estado">
                Consultando...
            </div>
        </div>

        <button id="boton-2"
                onclick="alternarLuz(2)">
            ...
        </button>

    </div>


    <div class="all-controls">

        <button id="boton-todas"
                onclick="alternarTodas()">
            ...
        </button>

    </div>

</section>


<script>

const estados = {
    1: null,
    2: null
};


function actualizarInterfaz(numero, encendida)
{
    estados[numero] = encendida;

    const estado =
        document.getElementById(`estado-${numero}`);

    const boton =
        document.getElementById(`boton-${numero}`);


    estado.textContent =
        encendida ? "Encendida" : "Apagada";

    estado.className =
        encendida
            ? "estado encendida"
            : "estado apagada";


    boton.textContent =
        encendida ? "Apagar" : "Encender";

    boton.className =
        encendida ? "off" : "on";


    actualizarBotonTodas();
}


function mostrarError(numero)
{
    estados[numero] = null;

    const estado =
        document.getElementById(`estado-${numero}`);

    const boton =
        document.getElementById(`boton-${numero}`);


    estado.textContent = "No disponible";
    estado.className = "estado error";

    boton.textContent = "No disponible";
    boton.disabled = true;

    actualizarBotonTodas();
}


function actualizarBotonTodas()
{
    const boton =
        document.getElementById("boton-todas");


    if (estados[1] === null ||
        estados[2] === null)
    {
        boton.textContent = "No disponible";
        boton.disabled = true;
        return;
    }


    boton.disabled = false;

    const todasEncendidas =
        estados[1] && estados[2];


    boton.textContent =
        todasEncendidas
            ? "Apagar todas"
            : "Encender todas";

    boton.className =
        todasEncendidas
            ? "off"
            : "on";
}


async function leerEstado(numero)
{
    try
    {
        const response =
            await fetch(
                `/lighting/${numero}`,
                {
                    cache: "no-store"
                });

        if (!response.ok)
            throw new Error();


        const estado =
            (await response.text()).trim();


        const boton =
            document.getElementById(`boton-${numero}`);

        boton.disabled = false;


        actualizarInterfaz(
            numero,
            estado === "on");
    }
    catch
    {
        mostrarError(numero);
    }
}


async function alternarLuz(numero)
{
    if (estados[numero] === null)
        return;


    const nuevoEstado =
        !estados[numero];

    const accion =
        nuevoEstado ? "on" : "off";

    const boton =
        document.getElementById(`boton-${numero}`);


    try
    {
        boton.disabled = true;


        const response =
            await fetch(
                `/lighting/${numero}/${accion}`,
                {
                    method: "PUT"
                });

        if (!response.ok)
            throw new Error();


        boton.disabled = false;

        actualizarInterfaz(
            numero,
            nuevoEstado);
    }
    catch
    {
        boton.disabled = false;

        alert(
            "No se pudo cambiar el estado de la luz."
        );

        leerEstado(numero);
    }
}


async function alternarTodas()
{
    if (estados[1] === null ||
        estados[2] === null)
    {
        return;
    }


    const todasEncendidas =
        estados[1] && estados[2];

    const nuevoEstado =
        !todasEncendidas;

    const accion =
        nuevoEstado ? "on" : "off";

    const boton =
        document.getElementById("boton-todas");


    try
    {
        boton.disabled = true;


        const response =
            await fetch(
                `/lighting/all/${accion}`,
                {
                    method: "PUT"
                });

        if (!response.ok)
            throw new Error();


        boton.disabled = false;

        actualizarInterfaz(
            1,
            nuevoEstado);

        actualizarInterfaz(
            2,
            nuevoEstado);
    }
    catch
    {
        boton.disabled = false;

        alert(
            "No se pudo cambiar el estado de las luces."
        );

        leerEstado(1);
        leerEstado(2);
    }
}


leerEstado(1);
leerEstado(2);

</script>

)HTML");
}