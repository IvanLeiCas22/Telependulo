# Calibración de cámaras

## Objetivo

Describir cómo Telepéndulo realiza y gestiona la calibración monocular
de cada cámara, qué condiciones debe cumplir una observación y cómo se
persisten los resultados.

## Alcance

- La calibración monocular de cada cámara se realiza por separado.
- La calibración estéreo es un proceso distinto y no forma parte de
  `CameraCalibration`.
- Cada instancia de `CameraCalibration` representa una única cámara.

## Patrón ChArUco

Configuración actual por defecto:

- 12 columnas de cuadrados.
- 10 filas de cuadrados.
- Lado del cuadrado: 10 mm.
- Lado del marcador: 7 mm.
- Diccionario: `DICT_6X6_250`.

Los patrones utilizados fueron generados con versiones modernas de OpenCV,
por lo que no se utiliza el modo legacy de ChArUco.

El número esperado de esquinas ChArUco es:

(12 - 1) × (10 - 1) = 99

## Sesión de observaciones

Las observaciones pertenecen únicamente a la sesión actual de calibración
y no se persisten.

La primera observación aceptada establece la resolución de la sesión.
Todas las observaciones posteriores deben utilizar la misma resolución.

Cambiar la configuración del patrón ChArUco elimina las observaciones
existentes, pero no invalida una calibración previamente guardada.

## Aceptación de una observación

Una imagen se procesa de la siguiente manera:

1. Se valida el formato de entrada.
2. Se detectan marcadores y esquinas ChArUco.
3. Se genera una imagen anotada con lo detectado.
4. Se comprueba que estén presentes todas las esquinas del tablero.
5. Se comprueba que los IDs sean válidos y no estén duplicados.
6. Las esquinas se almacenan ordenadas por ID.
7. Se compara la vista con las observaciones anteriores.
8. Si aporta suficiente información nueva, se agrega a la sesión.

Actualmente se exige detectar el tablero completo.

Actualmente se aceptan imágenes `CV_8U` de uno o tres canales, coherentes
con los formatos entregados por las cámaras y el pipeline actual.

## Diversidad de observaciones

Para evitar almacenar imágenes prácticamente equivalentes, cada nueva
observación se compara con todas las anteriores.

Se calcula el desplazamiento RMS de las esquinas correspondientes y se
normaliza utilizando la diagonal de la imagen.

Actualmente una observación se considera demasiado similar si el
desplazamiento normalizado es menor o igual al 2 % de la diagonal.

Este criterio podrá ajustarse experimentalmente durante las pruebas reales.

## Recomendación de zona

La imagen se divide conceptualmente en una grilla de 3 × 3. Para cada zona
se cuenta cuántas esquinas ChArUco de las observaciones aceptadas han caído
en ella.

La siguiente zona recomendada es la de menor cobertura acumulada. Si todavía
no hay observaciones, se recomienda comenzar por el centro.

Cuando varias zonas tienen la misma cobertura, se utiliza un desempate
determinista que prioriza primero las esquinas y luego los bordes antes que
el centro:

1. `TopLeft`
2. `BottomRight`
3. `TopRight`
4. `BottomLeft`
5. `Top`
6. `Bottom`
7. `Left`
8. `Right`
9. `Center`

La recomendación es orientativa y no condiciona la aceptación de una
observación. El tablero debe mantenerse completamente visible y también se
debe variar su inclinación y distancia entre capturas.

## Cantidad mínima de observaciones

Se establece un mínimo de 10 observaciones aceptadas antes de realizar una
calibración.

Además de la cantidad, se busca que las observaciones cubran distintas
zonas, posiciones y orientaciones del tablero.

## Modelo de cámara

La calibración monocular utilizará inicialmente el modelo estándar de OpenCV:

- `fx`
- `fy`
- `cx`
- `cy`
- `k1`
- `k2`
- `p1`
- `p2`
- `k3`

No se utiliza inicialmente el modelo racional con `k4`, `k5` y `k6`.
Podrá evaluarse más adelante si los resultados experimentales muestran
que el modelo estándar no es suficiente.

## Resultado de calibración

Una calibración monocular contiene:

- matriz intrínseca de cámara;
- coeficientes de distorsión;
- resolución de la imagen;
- error RMS global.

Durante el análisis también pueden obtenerse:

- error de reproyección por observación;
- desviaciones estándar de los parámetros intrínsecos.

Estas estadísticas sirven para evaluar la calidad de la calibración, pero
no se persisten actualmente.

### Cálculo y activación

`calibrate()` calcula las estadísticas de las observaciones y conserva
internamente un análisis pendiente. `getPendingAnalysis()` devuelve un
puntero de solo lectura a ese análisis, o `nullptr` si no existe. Un cálculo
fallido descarta el análisis pendiente anterior.

Agregar una observación aceptada, eliminarla, limpiar la sesión o cambiar
el patrón ChArUco descarta automáticamente el análisis pendiente. Esas
operaciones incrementan la revisión de la sesión (`getRevision()`); limpiar
la sesión incrementa la revisión incluso cuando está vacía, para invalidar
capturas que pudieran estar en curso.

Solo `savePendingCalibration()` permite guardar y activar el resultado
pendiente desde la API pública. `calibrate()` por sí mismo no reemplaza la
calibración vigente. `loadCalibration()` recupera una calibración previamente
guardada sin modificar las observaciones actuales.

Una consulta a un puntero devuelto por la clase debe realizarse mientras
su mutex externo esté bloqueado; no se debe conservar ese puntero después
de liberar el mutex. Esto permite revisar los resultados antes de guardarlos.

## Persistencia

Cada cámara guarda su calibración de forma independiente dentro de
`saves/calibrations/` (directorio local excluido de Git):

- `saves/calibrations/camera1.json`
- `saves/calibrations/camera2.json`

`main` intenta cargar ambas calibraciones al iniciar; la ausencia o el fallo
de carga se informa sin impedir el arranque. El directorio se crea al
guardar por primera vez.

El archivo almacena:

- `cameraMatrix`
- `distCoeffs`
- `imageWidth`
- `imageHeight`
- `rmsError`

Las matrices se normalizan internamente a `CV_64F`.

El guardado se realiza primero en un archivo temporal y luego se reemplaza
el archivo definitivo, para evitar dejar una calibración parcialmente
escrita.

## Estado de calibración en memoria

Una calibración cargada o guardada correctamente se considera la
calibración vigente.

Un fallo al cargar un archivo no reemplaza una calibración válida que ya
estuviera en memoria.

Eliminar la calibración:

- borra el archivo persistido si existe;
- invalida la calibración en memoria;
- no elimina las observaciones de la sesión actual.

## Responsabilidades

`CameraCalibration` se ocupa de:

- gestionar observaciones ChArUco;
- validar las vistas;
- calcular la calibración;
- analizar su calidad;
- cargar, guardar y eliminar resultados.

No se ocupa de:

- capturar imágenes de la cámara;
- decidir cuándo tomar automáticamente una nueva imagen;
- gestionar HTTP;
- controlar la interfaz web.

La captura automática y la interacción con el usuario se coordinan desde
capas superiores.

### Coordinación desde `main` (C2)

`main` construye una instancia `CameraCalibration` y un mutex de calibración
para cada cámara, separados de los mutex que protegen las cámaras físicas.
Los accesos a métodos y punteros internos de calibración se realizan bajo
su mutex correspondiente.

La función `capturarObservacionCalibracion()` registra la revisión actual,
adquiere un frame bajo el mutex de cámara y lo procesa posteriormente bajo
el mutex de calibración. Si la revisión cambió durante la adquisición,
descarta la imagen en vez de incorporarla a un estado distinto. Esto puede
suceder también cuando otra captura acepta una observación concurrentemente.
La adquisición y el procesamiento nunca mantienen ambos mutex a la vez.

Desde C3.1 esta función se invoca mediante `PUT /calibration/{id}/observe`.
La página `/calibration` (C3.2) permite realizar capturas manuales o
automáticas y consultar los resultados sin agregar responsabilidades HTTP
a la librería de calibración.

### Captura automática desde el navegador (C3.2)

La captura automática se controla desde JavaScript en la página de calibración.
El usuario elige la cámara y un intervalo de 0,5 a 120 segundos (3 s por
defecto), y luego inicia o detiene la adquisición.

El ciclo realiza una captura y espera su respuesta antes de programar la
siguiente con `setTimeout()`. Por lo tanto, el intervalo comienza al finalizar
la petición anterior, no marca un periodo exacto entre fotografías.
El navegador nunca envía dos capturas automáticas simultáneas.

Las observaciones rechazadas se muestran anotadas sin aumentar el contador.
Si cambia la resolución, se detiene el ciclo para que el usuario pueda
reiniciar las observaciones; también se detiene ante un fallo de adquisición.
La captura no se detiene automáticamente al alcanzar las 10 observaciones,
porque son un mínimo y no aseguran una calibración suficientemente diversa.

Al detener el ciclo se deja terminar la solicitud que ya estaba en curso;
las operaciones de cálculo y reinicio esperan a que termine esa captura.
Al cambiar de cámara, ocultar la pestaña o salir de la página, se detiene
el ciclo automático. No continúa si el navegador se cierra.

Los resultados pendientes deben revisarse y guardarse explícitamente:
no existe cálculo ni guardado automático.

## Checkpoints de integración

1. **C1 — Validación de la librería:** validadores reforzados y revisados.
2. **C2 — Coordinación:** análisis y revisión encapsulados en `CameraCalibration`; dos instancias y mutex independientes en `main`. Revisado.
3. **C3.1 — API HTTP:** rutas de calibración con respuestas JSON y PNG anotados. Revisado; contrato en `docs/http-api.md`.
4. **C3.2 — Interfaz web:** página `/calibration`, captura manual/automática configurable, visualización y guardado explícito. Implementado, pendiente de revisión.
5. **C3.3 — Validación integrada:** probar ambos flujos con cámaras reales, el stream concurrente y la persistencia tras reiniciar.

## Pendiente

- Evaluar `removeObservation(index)` para permitir eliminar una vista
  específica identificada como problemática mediante `perViewErrors`.
- Validar experimentalmente el umbral de similitud.
- Implementar calibración estéreo en un componente separado.

### Pruebas automatizadas (postergadas)

Prioridad posterior a la revisión e integración funcional de la calibración monocular.
Usar CMake/CTest, con fuentes en `tests/` y archivos generados únicamente en
un directorio de compilación ignorado por Git. Cubrir configuración, observaciones,
cálculo, persistencia y casos de error, sin requerir cámaras físicas.