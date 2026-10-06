#ifndef CAMERA_CALIBRATION_H
#define CAMERA_CALIBRATION_H

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <vector>
#include <cstddef>
#include <filesystem>

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

enum class CalibrationStatus
{
    Success,
    NotEnoughObservations,
    CalibrationFailed,
    WriteError
};

struct CharucoConfig
{
    int columns = 12;
    int rows = 10;
    float squareSize = 10.0f;
    float markerSize = 7.0f;
    CharucoDictionary dictionary = CharucoDictionary::Dict6x6_250;
};

struct MonocularCalibrationResult
{
    cv::Mat cameraMatrix;       // K
    cv::Mat distCoeffs;         // distortion coefficients (D)
    cv::Size imageSize{0,0};    // resolución imagenes
    double rmsError = 0.0;      // error de calibración en px
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

    // recibir una imagen de calibración y si es válida extraer la información importante
    bool addObservation(const cv::Mat& image, cv::Mat& annotatedImage);
    // obtener la cantidad de imagenes válidas con info guardada
    std::size_t getImageCount() const;

    // configurar el tipo de charuco esperado y limpiar las observaciones si se detectan cambios de configuración
    bool setCharucoConfig(const CharucoConfig& config);
    // devolver la configuración del charuco
    const CharucoConfig& getCharucoConfig() const;

    // cargar la calibración persistida desde archivo
    CalibrationLoadStatus loadCalibration();
    // realizar la calibración con todas las observaciones agregadas
    CalibrationStatus calibrate();
    // pedir los resultados de la calibración
    const MonocularCalibrationResult* getCalibrationResult() const;

    // borrar la calibración persistida de esta cámara
    bool clearCalibration();
    // borrar las observaciones guardadas
    void clearObservations();

private:
    struct CharucoObservation
    {
        std::vector<cv::Point2f> corners;
        std::vector<int> ids;
    };

    // funciones
    bool saveCalibration(const MonocularCalibrationResult& result);
    std::filesystem::path calibrationFilePath() const;

    // variables miembro
    std::vector<CharucoObservation> observations_;

    int cameraId_;
    std::filesystem::path calibrationDirectory_;

    cv::Size imageSize_{0, 0};    // Tamaño de imagen esperado
    CharucoConfig charucoConfig_;

    MonocularCalibrationResult calibrationResult_;    // Almacenar resultado de la última calibración
    bool isCalibrationValid_ = false;
};

#endif // CAMERA_CALIBRATION_H
