#include "cameracalibration.h"

#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/core/persistence.hpp>
#include <opencv2/objdetect/aruco_board.hpp>
#include <opencv2/objdetect/aruco_detector.hpp>
#include <opencv2/objdetect/aruco_dictionary.hpp>
#include <opencv2/objdetect/charuco_detector.hpp>

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace
{

constexpr double ObservationSimilarityThresholdFraction = 0.02;

bool getOpenCvDictionaryType(
    CharucoDictionary dictionary,
    cv::aruco::PredefinedDictionaryType& type)
{
    switch (dictionary)
    {
    case CharucoDictionary::Dict5x5_100:
        type = cv::aruco::DICT_5X5_100;
        return true;

    case CharucoDictionary::Dict6x6_250:
        type = cv::aruco::DICT_6X6_250;
        return true;

    default:
        return false;
    }
}

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

ObservationStatus CameraCalibration::addObservation(
    const cv::Mat& image,
    cv::Mat& annotatedImage)
{
    annotatedImage.release();

    // Solo se aceptan imágenes de 8 bits en escala de grises o BGR.
    if (image.empty() ||
        image.depth() != CV_8U ||
        (image.channels() != 1 && image.channels() != 3))
    {
        return ObservationStatus::InvalidImage;
    }

    try
    {
        // La salida anotada se genera aunque la observación luego sea rechazada.
        annotatedImage = image.clone();

        // Una sesión de observaciones mantiene una única resolución.
        if (imageSize_ != cv::Size{0, 0} && image.size() != imageSize_)
            return ObservationStatus::WrongImageSize;

        cv::aruco::PredefinedDictionaryType dictionaryType;

        if (!getOpenCvDictionaryType(charucoConfig_.dictionary, dictionaryType))
            return ObservationStatus::ProcessingFailed;

        const cv::aruco::Dictionary dictionary =
            cv::aruco::getPredefinedDictionary(dictionaryType);

        cv::aruco::CharucoBoard board(
            cv::Size(charucoConfig_.columns, charucoConfig_.rows),
            charucoConfig_.squareSize,
            charucoConfig_.markerSize,
            dictionary);

        // Los patrones utilizados son modernos, por lo que se mantiene
        // el comportamiento no-legacy por defecto de OpenCV.
        cv::aruco::CharucoDetector detector(board);

        std::vector<cv::Point2f> charucoCorners;
        std::vector<int> charucoIds;
        std::vector<std::vector<cv::Point2f>> markerCorners;
        std::vector<int> markerIds;

        detector.detectBoard(
            image,
            charucoCorners,
            charucoIds,
            markerCorners,
            markerIds);

        // Dibujar todo lo que pudo detectar OpenCV, incluso si el tablero
        // finalmente resulta incompleto.
        if (!markerCorners.empty())
        {
            cv::aruco::drawDetectedMarkers(
                annotatedImage,
                markerCorners,
                markerIds);
        }

        if (!charucoCorners.empty())
        {
            cv::aruco::drawDetectedCornersCharuco(
                annotatedImage,
                charucoCorners,
                charucoIds);
        }

        if (charucoCorners.size() != charucoIds.size())
            return ObservationStatus::ProcessingFailed;

        const std::size_t expectedCorners =
            board.getChessboardCorners().size();

        if (charucoCorners.size() != expectedCorners)
            return ObservationStatus::IncompleteBoard;

        CharucoObservation observation;
        observation.corners.resize(expectedCorners);
        observation.ids.resize(expectedCorners);

        std::vector<bool> seenIds(expectedCorners, false);

        // Guardar las esquinas en orden de ID para que la misma posición
        // represente siempre la misma esquina física del tablero.
        for (std::size_t i = 0; i < expectedCorners; ++i)
        {
            const int id = charucoIds[i];

            if (id < 0 ||
                static_cast<std::size_t>(id) >= expectedCorners ||
                seenIds[static_cast<std::size_t>(id)])
            {
                return ObservationStatus::ProcessingFailed;
            }

            const cv::Point2f& corner = charucoCorners[i];

            if (!std::isfinite(corner.x) || !std::isfinite(corner.y))
                return ObservationStatus::ProcessingFailed;

            const std::size_t index = static_cast<std::size_t>(id);
            observation.corners[index] = corner;
            observation.ids[index] = id;
            seenIds[index] = true;
        }

        if (isObservationTooSimilar(observation))
            return ObservationStatus::TooSimilar;

        // La resolución de la sesión se fija recién con la primera
        // observación efectivamente aceptada.
        if (observations_.empty())
            imageSize_ = image.size();

        observations_.push_back(std::move(observation));

        return ObservationStatus::Added;
    }
    catch (const cv::Exception&)
    {
        return ObservationStatus::ProcessingFailed;
    }
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

CalibrationTargetZone CameraCalibration::getRecommendedTargetZone() const
{
    if (observations_.empty() ||
        imageSize_.width <= 0 ||
        imageSize_.height <= 0)
    {
        return CalibrationTargetZone::Center;
    }

    // Cobertura acumulada de esquinas ChArUco en una grilla 3x3.
    std::size_t coverage[9] = {};

    const float firstColumnLimit =
        static_cast<float>(imageSize_.width) / 3.0f;
    const float secondColumnLimit =
        2.0f * static_cast<float>(imageSize_.width) / 3.0f;
    const float firstRowLimit =
        static_cast<float>(imageSize_.height) / 3.0f;
    const float secondRowLimit =
        2.0f * static_cast<float>(imageSize_.height) / 3.0f;

    for (const CharucoObservation& observation : observations_)
    {
        for (const cv::Point2f& corner : observation.corners)
        {
            const int column =
                corner.x < firstColumnLimit ? 0 :
                corner.x < secondColumnLimit ? 1 : 2;

            const int row =
                corner.y < firstRowLimit ? 0 :
                corner.y < secondRowLimit ? 1 : 2;

            ++coverage[row * 3 + column];
        }
    }

    // Ante igualdad de cobertura, favorecer zonas periféricas y alternadas
    // para dispersar las observaciones por el campo visual.
    static constexpr int priority[9] =
    {
        0, // TopLeft
        8, // BottomRight
        2, // TopRight
        6, // BottomLeft
        1, // Top
        7, // Bottom
        3, // Left
        5, // Right
        4  // Center
    };

    int bestIndex = priority[0];

    for (int i = 1; i < 9; ++i)
    {
        const int candidateIndex = priority[i];

        if (coverage[candidateIndex] < coverage[bestIndex])
            bestIndex = candidateIndex;
    }

    static constexpr CalibrationTargetZone zones[9] =
    {
        CalibrationTargetZone::TopLeft,
        CalibrationTargetZone::Top,
        CalibrationTargetZone::TopRight,
        CalibrationTargetZone::Left,
        CalibrationTargetZone::Center,
        CalibrationTargetZone::Right,
        CalibrationTargetZone::BottomLeft,
        CalibrationTargetZone::Bottom,
        CalibrationTargetZone::BottomRight
    };

    return zones[bestIndex];
}

CalibrationStatus CameraCalibration::calibrate(CalibrationAnalysis& analysis) const
{
    // Evitar que un fallo deje resultados anteriores visibles al llamador.
    analysis = CalibrationAnalysis{};

    if (observations_.size() < MinimumObservations)
        return CalibrationStatus::NotEnoughObservations;

    if (imageSize_.width <= 0 || imageSize_.height <= 0)
        return CalibrationStatus::CalibrationFailed;

    cv::aruco::PredefinedDictionaryType dictionaryType;

    if (!getOpenCvDictionaryType(charucoConfig_.dictionary, dictionaryType))
        return CalibrationStatus::CalibrationFailed;

    try
    {
        const cv::aruco::Dictionary dictionary =
            cv::aruco::getPredefinedDictionary(dictionaryType);

        cv::aruco::CharucoBoard board(
            cv::Size(charucoConfig_.columns, charucoConfig_.rows),
            charucoConfig_.squareSize,
            charucoConfig_.markerSize,
            dictionary);

        std::vector<std::vector<cv::Point3f>> objectPoints;
        std::vector<std::vector<cv::Point2f>> imagePoints;

        objectPoints.reserve(observations_.size());
        imagePoints.reserve(observations_.size());

        for (const CharucoObservation& observation : observations_)
        {
            if (observation.corners.empty() ||
                observation.corners.size() != observation.ids.size())
            {
                return CalibrationStatus::CalibrationFailed;
            }

            std::vector<cv::Point3f> viewObjectPoints;
            std::vector<cv::Point2f> viewImagePoints;

            board.matchImagePoints(
                observation.corners,
                observation.ids,
                viewObjectPoints,
                viewImagePoints);

            if (viewObjectPoints.empty() ||
                viewObjectPoints.size() != viewImagePoints.size())
            {
                return CalibrationStatus::CalibrationFailed;
            }

            objectPoints.push_back(std::move(viewObjectPoints));
            imagePoints.push_back(std::move(viewImagePoints));
        }

        cv::Mat cameraMatrix;
        cv::Mat distCoeffs;
        std::vector<cv::Mat> rvecs;
        std::vector<cv::Mat> tvecs;
        cv::Mat intrinsicStdDev;
        cv::Mat extrinsicStdDev;
        cv::Mat perViewErrors;

        const double rmsError = cv::calibrateCamera(
            objectPoints,
            imagePoints,
            imageSize_,
            cameraMatrix,
            distCoeffs,
            rvecs,
            tvecs,
            intrinsicStdDev,
            extrinsicStdDev,
            perViewErrors,
            0);

        CalibrationAnalysis calculatedAnalysis;
        calculatedAnalysis.result.cameraMatrix = std::move(cameraMatrix);
        calculatedAnalysis.result.distCoeffs = std::move(distCoeffs);
        calculatedAnalysis.result.imageSize = imageSize_;
        calculatedAnalysis.result.rmsError = rmsError;

        if (!isValidCalibrationResult(calculatedAnalysis.result) ||
            intrinsicStdDev.empty() ||
            intrinsicStdDev.channels() != 1 ||
            intrinsicStdDev.total() < 9 ||
            !cv::checkRange(intrinsicStdDev) ||
            perViewErrors.empty() ||
            perViewErrors.channels() != 1 ||
            perViewErrors.total() != observations_.size() ||
            !cv::checkRange(perViewErrors))
        {
            return CalibrationStatus::CalibrationFailed;
        }

        cv::Mat intrinsicStdDev64;
        intrinsicStdDev.convertTo(intrinsicStdDev64, CV_64F);
        intrinsicStdDev64 = intrinsicStdDev64.reshape(1, 1);

        const double* stdDev = intrinsicStdDev64.ptr<double>(0);

        for (int i = 0; i < 9; ++i)
        {
            if (!std::isfinite(stdDev[i]) || stdDev[i] < 0.0)
                return CalibrationStatus::CalibrationFailed;
        }

        calculatedAnalysis.intrinsicStdDev.fx = stdDev[0];
        calculatedAnalysis.intrinsicStdDev.fy = stdDev[1];
        calculatedAnalysis.intrinsicStdDev.cx = stdDev[2];
        calculatedAnalysis.intrinsicStdDev.cy = stdDev[3];
        calculatedAnalysis.intrinsicStdDev.k1 = stdDev[4];
        calculatedAnalysis.intrinsicStdDev.k2 = stdDev[5];
        calculatedAnalysis.intrinsicStdDev.p1 = stdDev[6];
        calculatedAnalysis.intrinsicStdDev.p2 = stdDev[7];
        calculatedAnalysis.intrinsicStdDev.k3 = stdDev[8];

        cv::Mat perViewErrors64;
        perViewErrors.convertTo(perViewErrors64, CV_64F);
        perViewErrors64 = perViewErrors64.reshape(1, 1);

        calculatedAnalysis.perViewErrors.reserve(observations_.size());

        for (std::size_t i = 0; i < observations_.size(); ++i)
        {
            const double error =
                perViewErrors64.at<double>(0, static_cast<int>(i));

            if (!std::isfinite(error) || error < 0.0)
                return CalibrationStatus::CalibrationFailed;

            calculatedAnalysis.perViewErrors.push_back(error);
        }

        // calibrate() solo calcula y analiza. No activa ni persiste el resultado.
        analysis = std::move(calculatedAnalysis);

        return CalibrationStatus::Success;
    }
    catch (const cv::Exception&)
    {
        return CalibrationStatus::CalibrationFailed;
    }
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
    const std::filesystem::path path = getCalibrationFilePath();

    // Si el archivo no existe, remove() devuelve false sin marcar error.
    std::error_code error;
    std::filesystem::remove(path, error);

    // Si hubo un error real de filesystem, conservar intacta
    // la calibración actualmente cargada en memoria.
    if (error)
        return false;

    // La calibración persistida ya no existe (o nunca existió),
    // por lo que tampoco debe quedar una calibración vigente en RAM.
    calibrationResult_ = MonocularCalibrationResult{};
    isCalibrationValid_ = false;

    return true;
}

bool CameraCalibration::isObservationTooSimilar(
    const CharucoObservation& observation) const
{
    if (observations_.empty() ||
        observation.corners.empty() ||
        imageSize_.width <= 0 ||
        imageSize_.height <= 0)
    {
        return false;
    }

    const double imageDiagonal =
        std::hypot(
            static_cast<double>(imageSize_.width),
            static_cast<double>(imageSize_.height));

    if (!std::isfinite(imageDiagonal) || imageDiagonal <= 0.0)
        return false;

    for (const CharucoObservation& previous : observations_)
    {
        if (previous.corners.size() != observation.corners.size())
            continue;

        double squaredDisplacementSum = 0.0;

        for (std::size_t i = 0; i < observation.corners.size(); ++i)
        {
            const double dx =
                static_cast<double>(observation.corners[i].x) -
                static_cast<double>(previous.corners[i].x);

            const double dy =
                static_cast<double>(observation.corners[i].y) -
                static_cast<double>(previous.corners[i].y);

            squaredDisplacementSum += dx * dx + dy * dy;
        }

        const double rmsDisplacement =
            std::sqrt(
                squaredDisplacementSum /
                static_cast<double>(observation.corners.size()));

        const double normalizedDisplacement =
            rmsDisplacement / imageDiagonal;

        if (normalizedDisplacement <= ObservationSimilarityThresholdFraction)
            return true;
    }

    return false;
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