# API HTTP de Telepéndulo

El servidor escucha en el puerto 8080. Actualmente admite `GET`, `PUT` y
streams MJPEG. La interacción de calibración se organiza mediante
`/calibration/{id}`, donde `id` es `1` o `2`.

## Calibración monocular (C3.1)

| Método | Ruta | Respuesta |
| --- | --- | --- |
| GET | `/calibration/{id}/status` | 200, estado JSON |
| PUT | `/calibration/{id}/observe` | 200, PNG anotado + cabeceras |
| PUT | `/calibration/{id}/undo` | 200, estado JSON |
| PUT | `/calibration/{id}/reset` | 200, estado JSON |
| PUT | `/calibration/{id}/calculate` | 200, estado JSON |
| PUT | `/calibration/{id}/save` | 200, estado JSON |
| PUT | `/calibration/{id}/clear-saved` | 200, estado JSON |

Las operaciones `PUT` no reciben cuerpo: la ruta determina la acción. Las
respuestas JSON y PNG incluyen `Cache-Control: no-store`. El servidor
acepta conexiones concurrentes, pero cada instancia de calibración está
protegida por su mutex independiente.

### Estado JSON

`GET /calibration/1/status` devuelve esta estructura (valores ilustrativos):

```json
{
  "cameraId": 1,
  "observationCount": 0,
  "minimumObservations": 10,
  "revision": 0,
  "recommendedZone": "center",
  "config": {
    "columns": 12,
    "rows": 10,
    "squareSize": 10,
    "markerSize": 7,
    "dictionary": "6x6_250"
  },
  "savedCalibration": null,
  "pendingAnalysis": null
}
```

- `recommendedZone`: `top_left`, `top`, `top_right`, `left`,
  `center`, `right`, `bottom_left`, `bottom` o `bottom_right`.
- `dictionary`: `5x5_100` o `6x6_250`.
- `savedCalibration`: `null` o un objeto con `imageWidth`,
  `imageHeight`, `rmsError`, `cameraMatrix` (matriz 3×3) y
  `distCoeffs` (arreglo de cinco coeficientes).
- `pendingAnalysis`: `null` o un objeto con `result` (mismo formato
  que `savedCalibration`), `intrinsicStdDev` (`fx`, `fy`, `cx`,
  `cy`, `k1`, `k2`, `p1`, `p2`, `k3`) y `perViewErrors`
  (arreglo en el orden de las observaciones aceptadas).

Los valores numéricos del análisis se serializan con precisión suficiente
para conservar los valores `double` del cálculo.

### Capturar observación

`PUT /calibration/{id}/observe` adquiere una imagen de la cámara y la
entrega como `image/png`, con las anotaciones de ChArUco, cuando la imagen
es válida para su visualización. Cabeceras adicionales:

```http
X-Observation-Status: added
X-Observation-Count: 1
X-Calibration-Revision: 1
```

`X-Observation-Status` puede ser `added`, `incomplete_board`,
`wrong_image_size` o `too_similar`. El estado es independiente del
código HTTP: por ejemplo, un tablero incompleto devuelve HTTP 200 porque
la captura fue procesada y existe una imagen anotada que mostrar.

Un error de adquisición o de procesamiento devuelve texto y un código de
error; no se entrega un PNG vacío. La captura se descarta con HTTP 409 si
cambió la revisión de la sesión mientras se adquiría. Esto también puede
ocurrir si otra observación fue aceptada concurrentemente.

La imagen no se almacena en `images/` ni en `saves/`.

### Otras operaciones

- `undo`: elimina la última observación aceptada. HTTP 409 si no existe.
- `reset`: elimina todas las observaciones y el análisis pendiente, pero
  **no** borra la calibración guardada. Incrementa la revisión incluso
  si las observaciones estaban vacías.
- `calculate`: exige un mínimo de 10 observaciones; calcula un análisis
  pendiente, sin activarlo ni persistirlo. HTTP 409 con cantidad
  insuficiente, HTTP 500 ante un fallo de cálculo.
- `save`: persiste y activa solo el análisis pendiente. HTTP 409 si no
  existe; HTTP 500 ante un error de escritura, preservando la calibración
  vigente anterior.
- `clear-saved`: borra `saves/calibrations/camera{id}.json` e invalida
  la calibración vigente. Conserva las observaciones y el análisis
  pendiente.

Cada operación satisfactoria, salvo `observe`, devuelve el estado JSON
actualizado. Los errores devuelven `text/plain`: HTTP 409 para conflictos
con el estado actual, HTTP 400 para imagen inválida sin anotación posible
y HTTP 500 para fallos internos o de adquisición.

### Ciclo de vida

Las observaciones y el análisis pendiente viven solo en RAM. Al aceptar
una observación, eliminarla, reiniciar la sesión o cambiar la configuración,
`CameraCalibration` invalida el análisis pendiente. Las calibraciones
guardadas se recuperan al iniciar y se almacenan bajo `saves/calibrations/`.

Las rutas son registradas desde `main`; `HttpServer` solo conoce métodos,
rutas, cuerpos y cabeceras HTTP. La interfaz web de calibración está en
`GET /calibration` (C3.2) y utiliza estas rutas para capturas manuales y
automáticas, estado, cálculo y persistencia. La temporización vive en el
navegador: las solicitudes `observe` se hacen secuencialmente, sin solaparlas.

## Rutas existentes

- `GET /calibration`: interfaz de calibración monocular (captura manual y automática, intervalo configurable, resultados y guardado explícito).
- `GET /`: página de cámaras.
- `GET /config`: página de iluminación.
- `GET /capture/{id}`: captura PNG guardada en `images/`.
- `GET /stream/{id}`: stream MJPEG.
- `GET /camera/{id}/controls` y `/modes`: información V4L2 (solo USB).
- `PUT /camera/{id}/control` y `/mode`: configuración V4L2 (solo USB).
- `GET /lighting/{id}` y `PUT /lighting/{id}/on|off`: iluminación.
- `PUT /lighting/all/on|off`: iluminación de todos los canales.

**Nota operativa:** las rutas HTTP actuales no implementan autenticación.
El servidor escucha en todas las interfaces IPv4; debe usarse en una red
de confianza y no exponerse directamente a Internet.
