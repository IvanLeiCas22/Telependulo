#include "calibrationpage.h"

std::string contenidoPaginaCalibracion()
{
    return R"HTML(
<style>
    .cal-root { max-width: 1200px; }
    .cal-toolbar { display: flex; flex-wrap: wrap; gap: 10px; margin-bottom: 18px; }
    .cal-tab { background: #e4e7eb; color: #222; }
    .cal-tab.selected { background: #2557a7; color: white; }
    .cal-section { background: white; padding: 20px; border-radius: 10px;
                   box-shadow: 0 2px 8px rgba(0,0,0,.08); margin-bottom: 18px; }
    .cal-section h2 { margin: 0 0 16px; font-size: 20px; }
    .cal-images { display: grid; grid-template-columns: repeat(2, minmax(0,1fr)); gap: 16px; }
    .cal-image-title { font-weight: bold; margin-bottom: 8px; }
    .cal-frame { position: relative; display: grid; place-items: center; aspect-ratio: 16/9;
                 background: #20252b; color: #e5e7eb; border-radius: 6px; overflow: hidden; }
    .cal-frame img { width: 100%; height: 100%; object-fit: contain; }
    .cal-frame [hidden] { display: none !important; }
    .cal-placeholder { text-align: center; padding: 12px; }
    .cal-controls { display: flex; flex-wrap: wrap; align-items: end; gap: 10px; }
    .cal-field { display: flex; flex-direction: column; gap: 5px; }
    .cal-field input { width: 140px; padding: 8px; border: 1px solid #b8bec8;
                       border-radius: 6px; font: inherit; }
    .cal-controls button { background: #e4e7eb; }
    .cal-controls button.primary { background: #2557a7; color: white; }
    .cal-controls button.danger { background: #f4e3e3; color: #922; }
    .cal-controls button:disabled, .cal-tab:disabled { opacity: .48; cursor: not-allowed; }
    .cal-message { margin: 16px 0 0; min-height: 20px; color: #3f4b5a; }
    .cal-message.error { color: #b42318; }
    .cal-message.success { color: #16803a; }
    .cal-metrics { display: grid; grid-template-columns: repeat(3, minmax(0,1fr)); gap: 12px; }
    .cal-metric { background: #f4f6f8; padding: 14px; border-radius: 6px; }
    .cal-metric small { display: block; color: #5c6470; margin-bottom: 6px; }
    .cal-metric strong { display: block; }
    .cal-progress { height: 7px; background: #e4e7eb; border-radius: 8px;
                    overflow: hidden; margin-top: 13px; }
    .cal-progress div { height: 100%; background: #2557a7; width: 0; }
    .cal-note { margin: 12px 0 0; font-size: 13px; color: #5c6470; }
    .cal-table { border-collapse: collapse; width: 100%; margin-top: 10px; }
    .cal-table th, .cal-table td { text-align: left; padding: 7px 10px;
                                  border-bottom: 1px solid #e0e3e8; }
    .cal-table td { font-variant-numeric: tabular-nums; }
    .cal-error-list { display: flex; flex-wrap: wrap; gap: 8px; padding: 0; list-style: none; }
    .cal-error-list li { padding: 7px 10px; background: #f4f6f8; border-radius: 6px; }
    @media (max-width: 900px) {
        .cal-images { grid-template-columns: 1fr; }
        .cal-metrics { grid-template-columns: 1fr; }
    }
</style>

<div class="cal-root">
    <div class="cal-toolbar" aria-label="Seleccionar cámara">
        <button type="button" class="cal-tab selected" id="cal-tab-1">Cámara 1</button>
        <button type="button" class="cal-tab" id="cal-tab-2">Cámara 2</button>
    </div>

    <section class="cal-section">
        <h2>Imágenes</h2>
        <div class="cal-images">
            <div>
                <div class="cal-image-title">Vista en vivo</div>
                <div class="cal-frame">
                    <img id="cal-live" alt="Vista en vivo de la cámara seleccionada" hidden>
                    <span id="cal-live-placeholder" class="cal-placeholder">Conectando con la cámara…</span>
                </div>
            </div>
            <div>
                <div class="cal-image-title">Última observación anotada</div>
                <div class="cal-frame">
                    <img id="cal-annotated" alt="Detecciones ChArUco de la última captura" hidden>
                    <span id="cal-annotated-placeholder" class="cal-placeholder">Todavía no hay capturas.</span>
                </div>
            </div>
        </div>
    </section>

    <section class="cal-section">
        <h2>Captura de observaciones</h2>
        <div class="cal-controls">
            <label class="cal-field" for="cal-interval">
                Intervalo (segundos)
                <input id="cal-interval" type="number" min="0.5" max="120" step="0.5" value="3">
            </label>
            <button type="button" class="primary" id="cal-start">Iniciar automático</button>
            <button type="button" id="cal-stop" disabled>Detener</button>
            <button type="button" id="cal-capture">Captura manual</button>
            <button type="button" id="cal-undo">Deshacer última</button>
            <button type="button" class="danger" id="cal-reset">Reiniciar observaciones</button>
        </div>
        <p id="cal-message" class="cal-message" role="status" aria-live="polite">
            Consultando estado de calibración…
        </p>
        <div class="cal-metrics">
            <div class="cal-metric">
                <small>Observaciones aceptadas</small>
                <strong id="cal-count">—</strong>
            </div>
            <div class="cal-metric">
                <small>Próxima zona recomendada</small>
                <strong id="cal-zone">—</strong>
            </div>
            <div class="cal-metric">
                <small>Patrón ChArUco</small>
                <strong id="cal-board">—</strong>
            </div>
        </div>
        <div class="cal-progress" aria-label="Progreso hasta el mínimo de observaciones">
            <div id="cal-progress"></div>
        </div>
        <p class="cal-note">Diez observaciones es el mínimo, no una garantía de calidad.
            Variá la inclinación, distancia y posición del tablero. Las capturas rechazadas no suman.</p>
    </section>

    <section class="cal-section">
        <h2>Análisis y calibración guardada</h2>
        <div class="cal-controls">
            <button type="button" class="primary" id="cal-calculate">Calcular</button>
            <button type="button" class="primary" id="cal-save">Guardar resultado</button>
            <button type="button" class="danger" id="cal-clear-saved">Eliminar calibración guardada</button>
        </div>
        <p id="cal-saved" class="cal-note">Calibración guardada: —</p>
        <div id="cal-results">
            <p class="cal-note">No hay análisis pendiente. Capturá al menos diez observaciones y pulsá Calcular.</p>
        </div>
    </section>
</div>

<script>
(function () {
    "use strict";

    const $ = (id) => document.getElementById(id);
    const cameraIds = [1, 2];
    const states = {1: null, 2: null};
    const images = {1: null, 2: null};
    const statusVersions = {1: 0, 2: 0};
    const zones = {
        top_left: "Superior izquierda", top: "Superior", top_right: "Superior derecha",
        left: "Izquierda", center: "Centro", right: "Derecha",
        bottom_left: "Inferior izquierda", bottom: "Inferior",
        bottom_right: "Inferior derecha"
    };
    const observations = {
        added: "Observación aceptada.",
        incomplete_board: "Tablero incompleto: intentá mantenerlo completamente visible.",
        wrong_image_size: "La resolución cambió: reiniciá las observaciones para continuar.",
        too_similar: "Observación demasiado similar: mové o incliná el tablero.",
        invalid_image: "La imagen recibida no es válida.",
        processing_failed: "Falló el procesamiento de ChArUco."
    };

    let selectedCamera = 1;
    let viewVersion = 0;
    let automatic = false;
    let timer = null;
    let capturePromise = null;
    let actionBusy = false;

    function number(value, digits = 3) {
        return Number.isFinite(value) ? Number(value).toFixed(digits) : "—";
    }

    function message(text, kind = "") {
        $("cal-message").textContent = text;
        $("cal-message").className = "cal-message" + (kind ? " " + kind : "");
    }

    function intervalSeconds() {
        const value = Number($("cal-interval").value);
        return Number.isFinite(value) && value >= 0.5 && value <= 120 ? value : null;
    }

    function updateControls() {
        const state = states[selectedCamera];
        const busy = actionBusy || capturePromise !== null;
        const available = state !== null;
        $("cal-start").disabled = automatic || busy || !available;
        $("cal-stop").disabled = !automatic;
        $("cal-interval").disabled = automatic;
        $("cal-capture").disabled = automatic || busy || !available;
        $("cal-undo").disabled = automatic || busy || !available || state.observationCount === 0;
        $("cal-reset").disabled = automatic || busy || !available;
        $("cal-calculate").disabled = automatic || busy || !available ||
            state.observationCount < state.minimumObservations;
        $("cal-save").disabled = automatic || busy || !available || !state.pendingAnalysis;
        $("cal-clear-saved").disabled = automatic || busy || !available || !state.savedCalibration;
        for (const id of cameraIds) {
            $("cal-tab-" + id).classList.toggle("selected", id === selectedCamera);
            $("cal-tab-" + id).setAttribute("aria-pressed", String(id === selectedCamera));
        }
    }

    function renderResults(state) {
        const container = $("cal-results");
        container.replaceChildren();

        if (!state || !state.pendingAnalysis) {
            const info = document.createElement("p");
            info.className = "cal-note";
            info.textContent = "No hay análisis pendiente. Capturá al menos diez observaciones y pulsá Calcular.";
            container.appendChild(info);
            return;
        }

        const pending = state.pendingAnalysis;
        const summary = document.createElement("p");
        summary.textContent = "RMS global: " + number(pending.result.rmsError) + " px. " +
            "Resolución: " + pending.result.imageWidth + " × " + pending.result.imageHeight + ".";
        container.appendChild(summary);

        const table = document.createElement("table");
        table.className = "cal-table";
        const head = document.createElement("tr");
        for (const title of ["Parámetro", "Valor", "Desviación estándar"]) {
            const cell = document.createElement("th");
            cell.textContent = title;
            head.appendChild(cell);
        }
        table.appendChild(head);

        const names = ["fx", "fy", "cx", "cy", "k1", "k2", "p1", "p2", "k3"];
        const K = pending.result.cameraMatrix;
        const D = pending.result.distCoeffs;
        const values = [K[0][0], K[1][1], K[0][2], K[1][2], ...D];
        names.forEach((name, i) => {
            const row = document.createElement("tr");
            for (const value of [name, number(values[i], 6),
                                 number(pending.intrinsicStdDev[name], 6)]) {
                const cell = document.createElement("td");
                cell.textContent = value;
                row.appendChild(cell);
            }
            table.appendChild(row);
        });
        container.appendChild(table);

        const title = document.createElement("p");
        title.textContent = "Error RMS por observación (px):";
        container.appendChild(title);

        const list = document.createElement("ul");
        list.className = "cal-error-list";
        pending.perViewErrors.forEach((value, index) => {
            const item = document.createElement("li");
            item.textContent = (index + 1) + ": " + number(value);
            list.appendChild(item);
        });
        container.appendChild(list);
    }

    function renderState() {
        const state = states[selectedCamera];
        if (!state) {
            $("cal-count").textContent = "—";
            $("cal-zone").textContent = "—";
            $("cal-board").textContent = "—";
            $("cal-progress").style.width = "0%";
            $("cal-saved").textContent = "Calibración guardada: —";
            renderResults(null);
            updateControls();
            return;
        }

        $("cal-count").textContent = state.observationCount + " / " +
            state.minimumObservations + " (mínimo)";
        $("cal-zone").textContent = zones[state.recommendedZone] || state.recommendedZone;
        $("cal-board").textContent = state.config.columns + " × " +
            state.config.rows + " (" + state.config.dictionary + ")";
        const progress = 100 * state.observationCount / state.minimumObservations;
        $("cal-progress").style.width = Math.min(100, progress) + "%";
        $("cal-saved").textContent = state.savedCalibration
            ? "Calibración guardada: RMS " + number(state.savedCalibration.rmsError) +
              " px — " + state.savedCalibration.imageWidth + " × " +
              state.savedCalibration.imageHeight
            : "No hay calibración guardada para esta cámara.";
        renderResults(state);
        updateControls();
    }

    function showAnnotatedImage(cameraId) {
        const img = $("cal-annotated");
        const url = images[cameraId];
        if (url) {
            img.src = url;
            img.hidden = false;
            $("cal-annotated-placeholder").hidden = true;
        } else {
            img.removeAttribute("src");
            img.hidden = true;
            $("cal-annotated-placeholder").hidden = false;
        }
    }

    function showLive(cameraId) {
        const img = $("cal-live");
        img.removeAttribute("src");
        img.hidden = false;
        $("cal-live-placeholder").hidden = true;
        img.src = "/stream/" + cameraId;
    }

    $("cal-live").addEventListener("error", () => {
        $("cal-live").hidden = true;
        $("cal-live-placeholder").textContent = "Vista en vivo no disponible.";
        $("cal-live-placeholder").hidden = false;
    });

    function stopAutomatic(showMessage = true) {
        const wasRunning = automatic;
        automatic = false;
        if (timer !== null) {
            clearTimeout(timer);
            timer = null;
        }
        updateControls();
        if (wasRunning && showMessage)
            message("Captura automática detenida.");
    }

    async function refreshState(cameraId) {
        const version = ++statusVersions[cameraId];
        const response = await fetch("/calibration/" + cameraId + "/status", {cache: "no-store"});
        if (!response.ok)
            throw new Error(await response.text() || "No se pudo consultar el estado.");
        const data = await response.json();
        if (version !== statusVersions[cameraId])
            return;
        states[cameraId] = data;
        if (cameraId === selectedCamera)
            renderState();
    }

    async function captureOnce() {
        if (capturePromise || actionBusy)
            return;
        const cameraId = selectedCamera;

        const task = (async () => {
            const response = await fetch("/calibration/" + cameraId + "/observe",
                                         {method: "PUT", cache: "no-store"});
            if (!response.ok)
                throw new Error(await response.text() || "No se pudo capturar la imagen.");

            const status = response.headers.get("X-Observation-Status");
            if (!status)
                throw new Error("El servidor no devolvió el estado de la observación.");

            const blob = await response.blob();
            if (blob.type !== "image/png")
                throw new Error("La respuesta no contiene una imagen PNG.");

            const url = URL.createObjectURL(blob);
            if (images[cameraId])
                URL.revokeObjectURL(images[cameraId]);
            images[cameraId] = url;

            if (cameraId === selectedCamera && !document.hidden) {
                showAnnotatedImage(cameraId);
                message(observations[status] || "Observación procesada.",
                        status === "added" ? "success" : "");
            }
            await refreshState(cameraId);
            return status;
        })();

        capturePromise = task;
        updateControls();
        try {
            return await task;
        } finally {
            if (capturePromise === task)
                capturePromise = null;
            updateControls();
        }
    }

    async function automaticCycle() {
        if (!automatic || document.hidden)
            return;

        try {
            const status = await captureOnce();
            if (status === "wrong_image_size") {
                stopAutomatic(false);
                message("Captura automática detenida: cambió la resolución. Reiniciá las observaciones.", "error");
                return;
            }
        } catch (error) {
            stopAutomatic(false);
            message(error.message || "Error durante la captura automática.", "error");
            return;
        }

        if (automatic)
            timer = setTimeout(automaticCycle, intervalSeconds() * 1000);
    }

    function startAutomatic() {
        if (automatic || capturePromise || actionBusy || !states[selectedCamera])
            return;
        if (intervalSeconds() === null) {
            message("Ingresá un intervalo entre 0,5 y 120 segundos.", "error");
            return;
        }
        if (document.hidden)
            return;

        automatic = true;
        message("Captura automática iniciada.");
        updateControls();
        void automaticCycle();
    }

    async function sendAction(name, successMessage, confirmation) {
        if (actionBusy || !states[selectedCamera])
            return;
        if (confirmation && !window.confirm(confirmation))
            return;

        stopAutomatic(false);
        actionBusy = true;
        updateControls();
        const cameraId = selectedCamera;
        const view = viewVersion;

        try {
            // La última captura debe terminar antes de calcular o reiniciar.
            if (capturePromise)
                await capturePromise.catch(() => {});

            const response = await fetch("/calibration/" + cameraId + "/" + name,
                                         {method: "PUT", cache: "no-store"});
            if (!response.ok)
                throw new Error(await response.text() || "No se pudo completar la operación.");

            const data = await response.json();
            ++statusVersions[cameraId];
            states[cameraId] = data;
            if (cameraId === selectedCamera && view === viewVersion) {
                renderState();
                message(successMessage, "success");
                if (name === "reset")
                    clearAnnotatedImage(cameraId);
            }
        } catch (error) {
            if (cameraId === selectedCamera && view === viewVersion)
                message(error.message || "Error de comunicación.", "error");
        } finally {
            actionBusy = false;
            updateControls();
        }
    }

    function clearAnnotatedImage(cameraId) {
        if (images[cameraId]) {
            URL.revokeObjectURL(images[cameraId]);
            images[cameraId] = null;
        }
        if (cameraId === selectedCamera)
            showAnnotatedImage(cameraId);
    }

    async function selectCamera(cameraId) {
        if (selectedCamera === cameraId)
            return;
        stopAutomatic(false);
        const view = ++viewVersion;
        selectedCamera = cameraId;
        showLive(cameraId);
        showAnnotatedImage(cameraId);
        renderState();
        message("Consultando la cámara " + cameraId + "…");
        try {
            await refreshState(cameraId);
            if (selectedCamera === cameraId && view === viewVersion)
                message("Cámara " + cameraId + " lista.");
        } catch (error) {
            if (selectedCamera === cameraId && view === viewVersion)
                message(error.message || "No se pudo consultar el estado.", "error");
        }
    }

    $("cal-tab-1").addEventListener("click", () => { void selectCamera(1); });
    $("cal-tab-2").addEventListener("click", () => { void selectCamera(2); });
    $("cal-start").addEventListener("click", startAutomatic);
    $("cal-stop").addEventListener("click", () => stopAutomatic());
    $("cal-capture").addEventListener("click", () => {
        const cameraId = selectedCamera;
        captureOnce().catch((error) => {
            if (selectedCamera === cameraId)
                message(error.message || "Error de captura.", "error");
        });
    });
    $("cal-undo").addEventListener("click", () =>
        void sendAction("undo", "Última observación eliminada."));
    $("cal-reset").addEventListener("click", () =>
        void sendAction("reset", "Observaciones reiniciadas.",
                        "¿Reiniciar todas las observaciones de esta cámara?"));
    $("cal-calculate").addEventListener("click", () =>
        void sendAction("calculate", "Análisis calculado. Revisá los resultados antes de guardar."));
    $("cal-save").addEventListener("click", () =>
        void sendAction("save", "Calibración guardada correctamente."));
    $("cal-clear-saved").addEventListener("click", () =>
        void sendAction("clear-saved", "Calibración guardada eliminada.",
                        "¿Eliminar la calibración persistida de esta cámara?"));

    document.addEventListener("visibilitychange", () => {
        if (document.hidden) {
            const wasAutomatic = automatic;
            stopAutomatic(false);
            $("cal-live").removeAttribute("src");
            if (wasAutomatic)
                message("Captura automática detenida al ocultar la pestaña.");
        } else {
            showLive(selectedCamera);
        }
    });
    window.addEventListener("pagehide", () => {
        stopAutomatic(false);
        $("cal-live").removeAttribute("src");
        for (const id of cameraIds)
            clearAnnotatedImage(id);
    });

    showLive(selectedCamera);
    updateControls();
    refreshState(selectedCamera)
        .then(() => {
            if (selectedCamera === 1 && viewVersion === 0)
                message("Cámara 1 lista.");
        })
        .catch((error) => {
            if (selectedCamera === 1 && viewVersion === 0)
                message(error.message || "No se pudo consultar el estado.", "error");
        });
})();
</script>
)HTML";
}
