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

`calibrate()` calcula un resultado y sus estadísticas utilizando las
observaciones actuales, pero no modifica automáticamente la calibración
vigente ni la persiste.

Una calibración pasa a ser la calibración vigente únicamente cuando se
guarda correctamente mediante `saveCalibration()` o cuando se carga
correctamente desde archivo.

Esto permite analizar un resultado antes de decidir si debe reemplazar
la calibración actual.

## Persistencia

Cada cámara guarda su calibración de forma independiente:

- `camera1.json`
- `camera2.json`

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

## Pendiente

- Evaluar `removeObservation(index)` para permitir eliminar una vista
  específica identificada como problemática mediante `perViewErrors`.
- Validar experimentalmente el umbral de similitud.
- Integrar el flujo con la interfaz web.
- Implementar calibración estéreo en un componente separado.