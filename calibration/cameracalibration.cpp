#include "cameracalibration.h"

#include <opencv2/core.hpp>
#include <opencv2/core/persistence.hpp>

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace
{

bool isValidCalibrationResult(const MonocularCalibrationResult& result)
{
    const bool cameraMatrixValid =
        !result.cameraMatrix.empty() &&
        result.cameraMatrix.rows == 3 &&
        result.cameraMatrix.cols == 3 &&
        result.cameraMatrix.channels() == 1 &&
        (result.cameraMatrix.depth() == CV_32F ||
         result.cameraMatrix.depth() == CV_64F) &&
        cv::checkRange(result.cameraMatrix);

    const bool distCoeffsValid =
        !result.distCoeffs.empty() &&
        result.distCoeffs.total() == 5 &&
        (result.distCoeffs.rows == 1 || result.distCoeffs.cols == 1) &&
        result.distCoeffs.channels() == 1 &&
        (result.distCoeffs.depth() == CV_32F ||
         result.distCoeffs.depth() == CV_64F) &&
        cv::checkRange(result.distCoeffs);

    if (!cameraMatrixValid ||
        !distCoeffsValid ||
        result.imageSize.width <= 0 ||
        result.imageSize.height <= 0 ||
        !std::isfinite(result.rmsError) ||
        result.rmsError < 0.0)
    {
        return false;
    }

    cv::Mat cameraMatrix64;
    result.cameraMatrix.convertTo(cameraMatrix64, CV_64F);

    const double fx = cameraMatrix64.at<double>(0, 0);
    const double fy = cameraMatrix64.at<double>(1, 1);

    return fx > 0.0 && fy > 0.0;
}

}

// Inicializar la identificación de la cámara y el directorio de calibración
CameraCalibration::CameraCalibration(
    int cameraId,
    const std::filesystem::path& calibrationDirectory)
    : cameraId_(cameraId),
    calibrationDirectory_(calibrationDirectory)
{

}

CalibrationLoadStatus CameraCalibration::loadCalibration()
{
    const std::filesystem::path path = getCalibrationFilePath();

    // Comprobar si el archivo existe sin lanzar excepciones de filesystem.
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);

    if (error)
        return CalibrationLoadStatus::ReadError;

    if (!exists)
        return CalibrationLoadStatus::NotFound;

    // Leer primero el archivo como texto para poder distinguir errores de lectura
    // de errores de formato JSON.
    std::ifstream file(path, std::ios::binary);

    if (!file.is_open())
        return CalibrationLoadStatus::ReadError;

    std::ostringstream buffer;
    buffer << file.rdbuf();

    if (!file.good() && !file.eof())
        return CalibrationLoadStatus::ReadError;

    const std::string content = buffer.str();

    if (content.empty())
        return CalibrationLoadStatus::InvalidFile;

    MonocularCalibrationResult loadedResult;

    try
    {
        cv::FileStorage storage(
            content,
            cv::FileStorage::READ |
            cv::FileStorage::MEMORY |
            cv::FileStorage::FORMAT_JSON);

        if (!storage.isOpened())
            return CalibrationLoadStatus::InvalidFile;

        const cv::FileNode cameraMatrixNode = storage["cameraMatrix"];
        const cv::FileNode distCoeffsNode = storage["distCoeffs"];
        const cv::FileNode imageWidthNode = storage["imageWidth"];
        const cv::FileNode imageHeightNode = storage["imageHeight"];
        const cv::FileNode rmsErrorNode = storage["rmsError"];

        // Todos los campos son obligatorios en la versión actual del archivo.
        if (cameraMatrixNode.empty() ||
            distCoeffsNode.empty() ||
            imageWidthNode.empty() ||
            imageHeightNode.empty() ||
            rmsErrorNode.empty())
        {
            return CalibrationLoadStatus::InvalidFile;
        }

        if (!imageWidthNode.isInt() ||
            !imageHeightNode.isInt() ||
            (!rmsErrorNode.isInt() && !rmsErrorNode.isReal()))
        {
            return CalibrationLoadStatus::InvalidFile;
        }

        cameraMatrixNode >> loadedResult.cameraMatrix;
        distCoeffsNode >> loadedResult.distCoeffs;

        int imageWidth = 0;
        int imageHeight = 0;

        imageWidthNode >> imageWidth;
        imageHeightNode >> imageHeight;
        rmsErrorNode >> loadedResult.rmsError;

        loadedResult.imageSize = {imageWidth, imageHeight};
    }
    catch (const cv::Exception&)
    {
        return CalibrationLoadStatus::InvalidFile;
    }

    if (!isValidCalibrationResult(loadedResult))
        return CalibrationLoadStatus::InvalidFile;

    // Normalizar las matrices a double para mantener un formato interno único.
    loadedResult.cameraMatrix.convertTo(loadedResult.cameraMatrix, CV_64F);
    loadedResult.distCoeffs.convertTo(loadedResult.distCoeffs, CV_64F);

    // Recién después de validar todo reemplazar la calibración vigente.
    calibrationResult_ = std::move(loadedResult);
    isCalibrationValid_ = true;

    return CalibrationLoadStatus::Loaded;
}

std::filesystem::path CameraCalibration::getCalibrationFilePath() const
{
    return calibrationDirectory_ / ("camera" + std::to_string(cameraId_) + ".json");
}

const CharucoConfig& CameraCalibration::getCharucoConfig() const
{
    return charucoConfig_;
}

std::size_t CameraCalibration::getObservationCount() const
{
    return observations_.size();
}

const MonocularCalibrationResult* CameraCalibration::getCalibrationResult() const
{
    if (!isCalibrationValid_)
        return nullptr;

    return &calibrationResult_;
}

bool CameraCalibration::saveCalibration(const MonocularCalibrationResult& result)
{
    // 1. Validar el resultado antes de tocar el estado o el filesystem.
    if (!isValidCalibrationResult(result))
        return false;

    // 2. Crear una copia independiente y normalizada a double.
    MonocularCalibrationResult savedResult;

    result.cameraMatrix.convertTo(savedResult.cameraMatrix, CV_64F);
    result.distCoeffs.convertTo(savedResult.distCoeffs, CV_64F);
    savedResult.imageSize = result.imageSize;
    savedResult.rmsError = result.rmsError;

    // 3. Serializar primero a JSON en memoria.
    std::string json;

    try
    {
        cv::FileStorage storage(
            ".json",
            cv::FileStorage::WRITE |
            cv::FileStorage::MEMORY |
            cv::FileStorage::FORMAT_JSON);

        if (!storage.isOpened())
            return false;

        storage << "cameraMatrix" << savedResult.cameraMatrix;
        storage << "distCoeffs" << savedResult.distCoeffs;
        storage << "imageWidth" << savedResult.imageSize.width;
        storage << "imageHeight" << savedResult.imageSize.height;
        storage << "rmsError" << savedResult.rmsError;

        json = storage.releaseAndGetString();
    }
    catch (const cv::Exception&)
    {
        return false;
    }

    if (json.empty())
        return false;

    const std::filesystem::path finalPath = getCalibrationFilePath();
    std::filesystem::path tempPath = finalPath;
    tempPath += ".tmp";

    // 4. Crear el directorio de calibración si todavía no existe.
    const std::filesystem::path directory = finalPath.parent_path();

    if (!directory.empty())
    {
        std::error_code error;
        std::filesystem::create_directories(directory, error);

        if (error)
            return false;
    }

    // 5. Escribir completamente el JSON en un archivo temporal.
    {
        std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);

        if (!file.is_open())
            return false;

        file.write(json.data(), static_cast<std::streamsize>(json.size()));
        file.close();

        if (!file)
        {
            std::error_code ignoredError;
            std::filesystem::remove(tempPath, ignoredError);
            return false;
        }
    }

    // 6. Reemplazar la calibración anterior solo después de una escritura exitosa.
    std::error_code renameError;
    std::filesystem::rename(tempPath, finalPath, renameError);

    if (renameError)
    {
        std::error_code ignoredError;
        std::filesystem::remove(tempPath, ignoredError);
        return false;
    }

    // 7. Recién ahora convertir el resultado guardado en la calibración vigente.
    calibrationResult_ = std::move(savedResult);
    isCalibrationValid_ = true;

    return true;
}

bool CameraCalibration::setCharucoConfig(const CharucoConfig& config)
{
    // 1. Validar configuración
    if (config.columns <= 3 ||
        config.rows <= 3 ||
        !std::isfinite(config.squareSize) ||
        !std::isfinite(config.markerSize) ||
        config.squareSize <= 0.0f ||
        config.markerSize <= 0.0f ||
        config.markerSize >= config.squareSize)
    {
        return false;
    }

    // Asegurarse de que el diccionario configurado sea uno de los esperados
    switch (config.dictionary)
    {
    case CharucoDictionary::Dict5x5_100:
    case CharucoDictionary::Dict6x6_250:
        break;

    default:
        return false;
    }

    // 2. Comprobar si la configuración realmente cambió
    const bool changed =
        config.columns != charucoConfig_.columns ||
        config.rows != charucoConfig_.rows ||
        config.squareSize != charucoConfig_.squareSize ||
        config.markerSize != charucoConfig_.markerSize ||
        config.dictionary != charucoConfig_.dictionary;

    // 3. Si no cambió, no hay nada que hacer
    if (!changed)
        return true;

    // 4. Guardar nueva configuración
    charucoConfig_ = config;

    // 5. Las observaciones anteriores ya no corresponden necesariamente
    // al mismo tablero
    clearObservations();

    // 6. Configuración aplicada correctamente
    return true;
}

bool CameraCalibration::clearCalibration()
{
    // 1. Obtener la ruta de la calibración

    // 2. Intentar borrar el archivo

    // 3. Si hubo un error real, devolver false
    //    y conservar intacta la calibración en RAM

    // 4. Si el archivo se borró o no existía,
    //    invalidar y limpiar la calibración en RAM

    // 5. return true
}

void CameraCalibration::clearObservations()
{
    observations_.clear();
    imageSize_ = {0, 0};
}

bool CameraCalibration::removeLastObservation()
{
    // No hay ninguna observación para eliminar
    if (observations_.empty())
        return false;

    // Eliminar la última observación agregada
    observations_.pop_back();

    // Si ya no quedan observaciones, tampoco queda una resolución
    // asociada a la sesión actual
    if (observations_.empty())
        imageSize_ = {0, 0};

    return true;
}