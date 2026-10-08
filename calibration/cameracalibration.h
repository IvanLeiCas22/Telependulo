#ifndef CAMERA_CALIBRATION_H
#define CAMERA_CALIBRATION_H

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <vector>
#include <cstddef>
#include <filesystem>
#include <cstdint>
#include <optional>

enum class CharucoDictionary
{
    Dict5x5_100,
    Dict6x6_250
};

enum class CalibrationLoadStatus
{
    Loaded,        // calibración encontrada y cargada correctamente
    NotFound,      // no existe calibración guardada
    InvalidFile,   // existe archivo pero su contenido no es válido
    ReadError      // hubo un error al intentar leerlo
};

enum class ObservationStatus
{
    Added,
    InvalidImage,
    IncompleteBoard,
    WrongImageSize,
    TooSimilar,
    ProcessingFailed
};

enum class CalibrationStatus
{
    Success,
    NotEnoughObservations,
    CalibrationFailed
};

enum class CalibrationTargetZone
{
    TopLeft,    Top,    TopRight,
    Left,       Center, Right,
    BottomLeft, Bottom, BottomRight
};

struct IntrinsicStandardDeviations
{
    double fx = 0.0;
    double fy = 0.0;
    double cx = 0.0;
    double cy = 0.0;

    double k1 = 0.0;
    double k2 = 0.0;
    double p1 = 0.0;
    double p2 = 0.0;
    double k3 = 0.0;
};

struct CharucoConfig
{
    int columns = 10;
    int rows = 7;
    float squareSize = 10.0f;   // mm
    float markerSize = 7.0f;    // mm
    CharucoDictionary dictionary = CharucoDictionary::Dict5x5_100;
};

struct MonocularCalibrationResult
{
    cv::Mat cameraMatrix;       // K
    cv::Mat distCoeffs;         // distortion coefficients (D)
    cv::Size imageSize{0,0};    // resolución imagenes
    double rmsError = 0.0;      // error de calibración en px
};

struct CalibrationAnalysis
{
    MonocularCalibrationResult result;
    IntrinsicStandardDeviations intrinsicStdDev;

    // un error RMS por cada observación, en el mismo orden en que fueron agregadas
    std::vector<double> perViewErrors;
};

class CameraCalibration
{
public:
    /**
     * constructor de la clase incorporando la identificación de la cámara y donde guardar su calibración
    */
    CameraCalibration(
        int cameraId,
        const std::filesystem::path& calibrationDirectory
        );

    /**
     * Funciones
    */

    // recibir una imagen de calibración y si es válida extraer la información importante
    ObservationStatus addObservation(const cv::Mat& image, cv::Mat& annotatedImage);
    // se elimina la última observación (motivos varios)
    bool removeLastObservation();
    // obtener la cantidad de observaciones guardadas
    std::size_t getObservationCount() const;
    // obtener la zona recomendada para la próxima observación
    CalibrationTargetZone getRecommendedTargetZone() const;

    // configurar el tipo de charuco esperado y limpiar las observaciones si se detectan cambios de configuración
    bool setCharucoConfig(const CharucoConfig& config);
    // devolver la configuración del charuco
    const CharucoConfig& getCharucoConfig() const;

    // cargar la calibración persistida desde archivo
    CalibrationLoadStatus loadCalibration();
    // calcular la calibración y conservar el análisis pendiente para su revisión
    CalibrationStatus calibrate();
    // consultar el último análisis pendiente, sin transferir su propiedad
    const CalibrationAnalysis* getPendingAnalysis() const;
    // guardar y activar exclusivamente el resultado pendiente de esta sesión
    bool savePendingCalibration();
    // revisión de la sesión para descartar capturas adquiridas antes de un reinicio
    std::uint64_t getRevision() const;
    // obtener la calibración actualmente cargada o guardada
    const MonocularCalibrationResult* getCalibrationResult() const;

    // borrar la calibración persistida de esta cámara
    bool clearCalibration();
    // borrar las observaciones guardadas
    void clearObservations();

    /**
     * Variables
    */

    static constexpr std::size_t MinimumObservations = 10;

private:
    struct CharucoObservation
    {
        std::vector<cv::Point2f> corners;
        std::vector<int> ids;
    };

    /**
     * Funciones
    */

    // obtener el filepath donde se guarda la calibración
    std::filesystem::path getCalibrationFilePath() const;

    // analizar si la observación obtenida es muy similar a las ya realizadas
    bool isObservationTooSimilar(const CharucoObservation& observation) const;
    // escribir el resultado validado y convertirlo en calibración vigente
    bool saveCalibration(const MonocularCalibrationResult& result);
    // invalidar el análisis cuando cambie la sesión de observaciones
    void invalidatePendingAnalysis();

    // variables miembro
    std::vector<CharucoObservation> observations_;

    int cameraId_;
    std::filesystem::path calibrationDirectory_;

    cv::Size imageSize_{0, 0};    // Tamaño de imagen esperado
    CharucoConfig charucoConfig_;
    std::optional<CalibrationAnalysis> pendingAnalysis_;
    std::uint64_t revision_ = 0;

    MonocularCalibrationResult calibrationResult_;    // Calibración actualmente cargada o guardada
    bool isCalibrationValid_ = false;
};

#endif // CAMERA_CALIBRATION_H
