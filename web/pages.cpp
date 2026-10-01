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
            position: sticky;
            top: 0;
            align-self: flex-start;
            width: 220px;
            height: 100vh;
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

        .panel + .panel {
            margin-top: 20px;
        }

        .v4l2-row {
            display: flex;
            align-items: center;
            justify-content: space-between;
            gap: 20px;
            padding: 12px 0;
            border-bottom: 1px solid #ddd;
        }

        .v4l2-row:last-child {
            border-bottom: none;
        }

        .v4l2-row.inactive {
            opacity: 0.45;
        }

        .v4l2-input {
            width: 180px;
        }

        .v4l2-row input[type="checkbox"] {
            width: auto;
        }

        .camera-grid {
            display: grid;
            grid-template-columns: repeat(2, minmax(0, 1fr));
            gap: 20px;
            width: 100%;
        }

        .camera-grid.single-view {
            grid-template-columns: 1fr;
        }

        .camera-panel {
            padding: 20px;
            border-radius: 10px;
            background: white;
            box-shadow: 0 2px 8px rgba(0, 0, 0, 0.08);
        }

        .camera-panel h2 {
            margin-top: 0;
        }

        .camera-controls {
            display: flex;
            gap: 8px;
        }

        .camera-mode {
            min-width: 90px;
            background: #d6d6d6;
            color: #222;
        }

        .camera-layout {
            display: block;
        }

        .camera-view {
            min-width: 0;
        }

        .camera-image {
            display: none;
            width: 100%;
            margin-top: 18px;
            border-radius: 6px;
        }

        .camera-settings {
            margin-top: 18px;
            max-height: calc(100vh - 520px);
            overflow-y: auto;
            padding-right: 4px;
        }

        .camera-settings h3 {
            margin-top: 0;
        }

        @media (max-width: 900px) {
            .camera-grid {
                grid-template-columns: 1fr;
            }
        }
    </style>
</head>

<body>

<div class="hmi">

    <nav class="sidebar">
        <h2>Telepéndulo</h2>

        <a href="/">Camaras</a>
        <a href="/config">Iluminación</a>
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

std::string paginaCamaras()
{
    return crearPagina(
        "Camaras",

        R"HTML(

<section id="camera-grid" class="camera-grid">

    <div id="panel-1" class="camera-panel">
        <h2>Cámara 1</h2>

        <div class="camera-controls">
            <button id="modo-1" class="camera-mode" onclick="alternarModo(1)">Captura</button>
            <button id="accion-1" class="on" onclick="ejecutarCamara(1)">Iniciar</button>
        </div>

        <div class="camera-layout">
            <div class="camera-view">
                <img id="imagen-1" class="camera-image" alt="Cámara 1">
            </div>

            <div id="configuracion-camara-1" class="camera-settings">
                <h3>Configuración</h3>

                <div class="v4l2-row">
                    <span>Resolución</span>
                    <select id="resolucion-1" class="v4l2-input"></select>
                </div>

                <div class="v4l2-row">
                    <span>FPS</span>
                    <select id="fps-1" class="v4l2-input"></select>
                </div>

                <div id="estado-modos-1" class="estado">Consultando modos...</div>
                <div id="controles-camara-1"></div>
            </div>
        </div>
    </div>

    <div id="panel-2" class="camera-panel">
        <h2>Cámara 2</h2>

        <div class="camera-controls">
            <button id="modo-2" class="camera-mode" onclick="alternarModo(2)">Captura</button>
            <button id="accion-2" class="on" onclick="ejecutarCamara(2)">Iniciar</button>
        </div>

        <div class="camera-layout">
            <div class="camera-view">
                <img id="imagen-2" class="camera-image" alt="Cámara 2">
            </div>

            <div id="configuracion-camara-2" class="camera-settings">
                <h3>Configuración</h3>

                <div class="v4l2-row">
                    <span>Resolución</span>
                    <select id="resolucion-2" class="v4l2-input"></select>
                </div>

                <div class="v4l2-row">
                    <span>FPS</span>
                    <select id="fps-2" class="v4l2-input"></select>
                </div>

                <div id="estado-modos-2" class="estado">Consultando modos...</div>
                <div id="controles-camara-2"></div>
            </div>
        </div>
    </div>

</section>

<script>

const modosCamara = {
    1: "capture",
    2: "capture"
};

const streamsActivos = {
    1: false,
    2: false
};

const capturas = {
    1: null,
    2: null
};

function actualizarDistribucion()
{
    const visibles = [1, 2].filter(numero => {
        return document.getElementById(`imagen-${numero}`).hasAttribute("src");
    }).length;

    document.getElementById("camera-grid").classList.toggle("single-view", visibles === 1);
}

function limpiarImagen(numero)
{
    const imagen = document.getElementById(`imagen-${numero}`);

    if (capturas[numero])
    {
        URL.revokeObjectURL(capturas[numero]);
        capturas[numero] = null;
    }

    imagen.removeAttribute("src");
    imagen.style.display = "none";

    actualizarDistribucion();
}

function detenerStream(numero)
{
    if (!streamsActivos[numero])
        return;

    streamsActivos[numero] = false;
    limpiarImagen(numero);

    const boton = document.getElementById(`accion-${numero}`);
    boton.textContent = "Iniciar";
    boton.className = "on";
}

function alternarModo(numero)
{
    if (streamsActivos[numero])
        detenerStream(numero);
    else
        limpiarImagen(numero);

    modosCamara[numero] = modosCamara[numero] === "capture" ? "stream" : "capture";

    const botonModo = document.getElementById(`modo-${numero}`);
    botonModo.textContent = modosCamara[numero] === "capture" ? "Captura" : "Stream";
}

async function capturar(numero)
{
    const boton = document.getElementById(`accion-${numero}`);
    const imagen = document.getElementById(`imagen-${numero}`);

    boton.disabled = true;

    try
    {
        const response = await fetch(`/capture/${numero}`, {cache: "no-store"});

        if (!response.ok)
            throw new Error();

        const blob = await response.blob();

        if (capturas[numero])
            URL.revokeObjectURL(capturas[numero]);

        capturas[numero] = URL.createObjectURL(blob);
        imagen.src = capturas[numero];
        imagen.style.display = "block";

        actualizarDistribucion();
    }
    catch
    {
        alert("No se pudo capturar la imagen.");
    }

    boton.disabled = false;
}

function iniciarStream(numero)
{
    const imagen = document.getElementById(`imagen-${numero}`);
    const boton = document.getElementById(`accion-${numero}`);

    streamsActivos[numero] = true;
    imagen.src = `/stream/${numero}`;
    imagen.style.display = "block";

    actualizarDistribucion();

    boton.textContent = "Detener";
    boton.className = "off";
}

function ejecutarCamara(numero)
{
    if (modosCamara[numero] === "capture")
    {
        capturar(numero);
        return;
    }

    if (streamsActivos[numero])
        detenerStream(numero);
    else
        iniciarStream(numero);
}

const modosDisponibles = {
    1: [],
    2: []
};

function actualizarFps(numero)
{
    const resolucion = document.getElementById(`resolucion-${numero}`);
    const fps = document.getElementById(`fps-${numero}`);
    const modo = resolucion.value;

    fps.replaceChildren();

    for (const item of modosDisponibles[numero])
    {
        if (`${item.width}x${item.height}` !== modo)
            continue;

        const option = document.createElement("option");
        option.value = item.fps;
        option.textContent = `${item.fps} FPS`;
        fps.appendChild(option);
    }
}

async function cambiarModoCamara(numero)
{
    const resolucion = document.getElementById(`resolucion-${numero}`);
    const fps = document.getElementById(`fps-${numero}`);

    const [width, height] = resolucion.value.split("x");

    resolucion.disabled = true;
    fps.disabled = true;

    try
    {
        const response = await fetch(
            `/camera/${numero}/mode?width=${width}&height=${height}&fps=${fps.value}`,
            {method: "PUT"}
        );

        if (!response.ok)
            throw new Error();

        await cargarControlesCamara(numero);
        resolucion.disabled = false;
        fps.disabled = false;
    }
    catch
    {
        alert("No se pudo cambiar el modo de la cámara.");
        await cargarModosCamara(numero);
        await cargarControlesCamara(numero);
    }
}

async function cargarModosCamara(numero)
{
    const configuracion = document.getElementById(`configuracion-camara-${numero}`);
    const estado = document.getElementById(`estado-modos-${numero}`);
    const resolucion = document.getElementById(`resolucion-${numero}`);
    const fps = document.getElementById(`fps-${numero}`);

    try
    {
        const response = await fetch(`/camera/${numero}/modes`, {cache: "no-store"});

        if (response.status === 404)
        {
            configuracion.style.display = "none";
            return false;
        }

        if (!response.ok)
            throw new Error();

        configuracion.style.display = "block";

        modosDisponibles[numero] = await response.json();
        const resoluciones = [];

        for (const item of modosDisponibles[numero])
        {
            const value = `${item.width}x${item.height}`;

            if (!resoluciones.includes(value))
                resoluciones.push(value);
        }

        resolucion.replaceChildren();

        for (const value of resoluciones)
        {
            const option = document.createElement("option");
            option.value = value;
            option.textContent = value;
            resolucion.appendChild(option);
        }

        resolucion.onchange = () => {
            actualizarFps(numero);
            cambiarModoCamara(numero);
        };

        fps.onchange = () => cambiarModoCamara(numero);

        resolucion.disabled = false;
        fps.disabled = false;
        actualizarFps(numero);
        estado.style.display = "none";
        return true;
    }
    catch
    {
        configuracion.style.display = "block";
        estado.textContent = "No se pudieron consultar los modos.";
        estado.className = "estado error";
        resolucion.disabled = true;
        fps.disabled = true;
        return false;
    }
}

function crearEntradaControl(numero, control)
{
    let entrada;

    if (control.type === "integer")
    {
        entrada = document.createElement("input");
        entrada.type = "number";
        entrada.value = control.value;
        entrada.min = control.min;
        entrada.max = control.max;
        entrada.step = control.step;
    }
    else if (control.type === "boolean")
    {
        entrada = document.createElement("input");
        entrada.type = "checkbox";
        entrada.checked = control.value !== 0;
    }
    else
    {
        entrada = document.createElement("select");

        for (const option of control.options)
        {
            const item = document.createElement("option");
            item.value = option.value;
            item.textContent = option.name;
            entrada.appendChild(item);
        }

        entrada.value = control.value;
    }

    entrada.className = "v4l2-input";
    entrada.disabled = control.inactive;
    entrada.addEventListener("change", () => cambiarControlCamara(numero, control, entrada));
    return entrada;
}

async function cambiarControlCamara(numero, control, entrada)
{
    const value = control.type === "boolean" ? (entrada.checked ? 1 : 0) : entrada.value;
    entrada.disabled = true;

    try
    {
        const response = await fetch(
            `/camera/${numero}/control?id=${control.id}&value=${value}`,
            {method: "PUT"}
        );

        if (!response.ok)
            throw new Error();
    }
    catch
    {
        alert("No se pudo cambiar el control de la cámara.");
    }

    await cargarControlesCamara(numero);
}

async function cargarControlesCamara(numero)
{
    const contenedor = document.getElementById(`controles-camara-${numero}`);

    try
    {
        const response = await fetch(`/camera/${numero}/controls`, {cache: "no-store"});

        if (!response.ok)
            throw new Error();

        const controls = await response.json();
        contenedor.replaceChildren();

        for (const control of controls)
        {
            const row = document.createElement("div");
            row.className = control.inactive ? "v4l2-row inactive" : "v4l2-row";

            const name = document.createElement("span");
            name.textContent = control.name;

            row.appendChild(name);
            row.appendChild(crearEntradaControl(numero, control));
            contenedor.appendChild(row);
        }

        if (controls.length === 0)
        {
            const mensaje = document.createElement("div");
            mensaje.className = "estado";
            mensaje.textContent = "No hay controles disponibles.";
            contenedor.appendChild(mensaje);
        }
    }
    catch
    {
        const mensaje = document.createElement("div");
        mensaje.className = "estado error";
        mensaje.textContent = "No se pudieron consultar los controles.";
        contenedor.replaceChildren(mensaje);
    }
}

async function cargarConfiguracionCamara(numero)
{
    if (await cargarModosCamara(numero))
        await cargarControlesCamara(numero);
}

cargarConfiguracionCamara(1);
cargarConfiguracionCamara(2);

</script>

)HTML");
}

std::string paginaConfiguracion()
{
    return crearPagina(
        "Iluminación",

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