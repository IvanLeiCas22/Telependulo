#include "cameracalibration.h"

#include <opencv2/calib3d.hpp>
#include <opencv2/core/persistence.hpp>
#include <opencv2/objdetect/aruco_detector.hpp>
#include <opencv2/objdetect/charuco_detector.hpp>

#include <cmath>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace
{
constexpr std::size_t MIN_OBSERVATIONS = 10;

bool getOpenCvDictionary(CharucoDictionary dictionaryId, cv::aruco::Dictionary& dictionary)
{
    switch (dictionaryId)
    {
    case CharucoDictionary::Dict5x5_100:
        dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_5X5_100);
        return true;

    case CharucoDictionary::Dict6x6_250:
        dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_250);
        return true;
    }

    return false;
}

bool sameCharucoConfig(const CharucoConfig& a, const CharucoConfig& b)
{
    return a.columns == b.columns &&
           a.rows == b.rows &&
           a.squareSize == b.squareSize &&
           a.markerSize == b.markerSize &&
           a.dictionary == b.dictionary;
}

bool isValidCharucoConfig(const CharucoConfig& config)
{
    if (config.columns < 2 ||
        config.rows < 2 ||
        !std::isfinite(config.squareSize) ||
        !std::isfinite(config.markerSize) ||
        config.squareSize <= 0.0f ||
        config.markerSize <= 0.0f ||
        config.markerSize >= config.squareSize)
    {
        return false;
    }

    cv::aruco::Dictionary dictionary;
    if (!getOpenCvDictionary(config.dictionary, dictionary))
        return false;

    try
    {
        cv::aruco::CharucoBoard board(
            cv::Size(config.columns, config.rows),
            config.squareSize,
            config.markerSize,
            dictionary
            );

        return !board.getChessboardCorners().empty();
    }
    catch (const cv::Exception&)
    {
        return false;
    }
}

bool isFiniteMat(const cv::Mat& matrix)
{
    if (matrix.empty() || matrix.channels() != 1)
        return false;

    return cv::checkRange(matrix, true, nullptr);
}

bool isValidCalibrationResult(const MonocularCalibrationResult& result)
{
    if (result.cameraMatrix.rows != 3 ||
        result.cameraMatrix.cols != 3 ||
        !isFiniteMat(result.cameraMatrix))
    {
        return false;
    }

    if (!isFiniteMat(result.distCoeffs) ||
        (result.distCoeffs.rows != 1 && result.distCoeffs.cols != 1) ||
        result.distCoeffs.total() < 4)
    {
        return false;
    }

    if (result.imageSize.width <= 0 || result.imageSize.height <= 0)
        return false;

    if (!std::isfinite(result.rmsError) || result.rmsError < 0.0)
        return false;

    return true;
}

CalibrationLoadStatus readCalibrationFile(
    const std::filesystem::path& path,
    MonocularCalibrationResult& result)
{
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);

    if (error)
        return CalibrationLoadStatus::ReadError;

    if (!exists)
        return CalibrationLoadStatus::NotFound;

    std::ifstream probe(path, std::ios::binary);
    if (!probe)
        return CalibrationLoadStatus::ReadError;
    probe.close();

    try
    {
        cv::FileStorage storage(
            path.string(),
            cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON
            );

        if (!storage.isOpened())
            return CalibrationLoadStatus::InvalidFile;

        const cv::FileNode cameraMatrixNode = storage["cameraMatrix"];
        const cv::FileNode distCoeffsNode = storage["distCoeffs"];
        const cv::FileNode imageWidthNode = storage["imageWidth"];
        const cv::FileNode imageHeightNode = storage["imageHeight"];
        const cv::FileNode rmsErrorNode = storage["rmsError"];

        if (cameraMatrixNode.empty() ||
            distCoeffsNode.empty() ||
            imageWidthNode.empty() ||
            imageHeightNode.empty() ||
            rmsErrorNode.empty())
        {
            return CalibrationLoadStatus::InvalidFile;
        }

        MonocularCalibrationResult loadedResult;

        cameraMatrixNode >> loadedResult.cameraMatrix;
        distCoeffsNode >> loadedResult.distCoeffs;

        int imageWidth = 0;
        int imageHeight = 0;

        imageWidthNode >> imageWidth;
        imageHeightNode >> imageHeight;
        rmsErrorNode >> loadedResult.rmsError;

        loadedResult.imageSize = cv::Size(imageWidth, imageHeight);

        if (!isValidCalibrationResult(loadedResult))
            return CalibrationLoadStatus::InvalidFile;

        result = std::move(loadedResult);
        return CalibrationLoadStatus::Loaded;
    }
    catch (const cv::Exception&)
    {
        return CalibrationLoadStatus::InvalidFile;
    }
}

bool hasAllExpectedCharucoCorners(
    const std::vector<cv::Point2f>& corners,
    const std::vector<int>& ids,
    int expectedCount)
{
    if (expectedCount <= 0 ||
        static_cast<int>(corners.size()) != expectedCount ||
        static_cast<int>(ids.size()) != expectedCount)
    {
        return false;
    }

    std::vector<bool> seen(static_cast<std::size_t>(expectedCount), false);

    for (int id : ids)
    {
        if (id < 0 || id >= expectedCount)
            return false;

        if (seen[static_cast<std::size_t>(id)])
            return false;

        seen[static_cast<std::size_t>(id)] = true;
    }

    for (bool detected : seen)
    {
        if (!detected)
            return false;
    }

    return true;
}
}

CameraCalibration::CameraCalibration(
    int cameraId,
    const std::filesystem::path& calibrationDirectory)
    : cameraId_(cameraId),
    calibrationDirectory_(calibrationDirectory)
{
}

bool CameraCalibration::addObservation(
    const cv::Mat& image,
    cv::Mat& annotatedImage)
{
    annotatedImage.release();

    if (image.empty())
        return false;

    if (image.channels() != 1 && image.channels() != 3)
        return false;

    annotatedImage = image.clone();

    cv::aruco::Dictionary dictionary;
    if (!getOpenCvDictionary(charucoConfig_.dictionary, dictionary))
        return false;

    try
    {
        const cv::aruco::CharucoBoard board(
            cv::Size(charucoConfig_.columns, charucoConfig_.rows),
            charucoConfig_.squareSize,
            charucoConfig_.markerSize,
            dictionary
            );

        const cv::aruco::CharucoDetector detector(board);

        std::vector<cv::Point2f> charucoCorners;
        std::vector<int> charucoIds;
        std::vector<std::vector<cv::Point2f>> markerCorners;
        std::vector<int> markerIds;

        detector.detectBoard(
            image,
            charucoCorners,
            charucoIds,
            markerCorners,
            markerIds
            );

        if (!markerIds.empty())
            cv::aruco::drawDetectedMarkers(annotatedImage, markerCorners, markerIds);

        if (!charucoIds.empty())
        {
            cv::aruco::drawDetectedCornersCharuco(
                annotatedImage,
                charucoCorners,
                charucoIds
                );
        }

        const int expectedCorners =
            (charucoConfig_.columns - 1) * (charucoConfig_.rows - 1);

        if (!hasAllExpectedCharucoCorners(
                charucoCorners,
                charucoIds,
                expectedCorners))
        {
            return false;
        }

        if (imageSize_.width != 0 || imageSize_.height != 0)
        {
            if (image.size() != imageSize_)
                return false;
        }

        if (imageSize_.width == 0 && imageSize_.height == 0)
            imageSize_ = image.size();

        observations_.push_back({std::move(charucoCorners), std::move(charucoIds)});
        return true;
    }
    catch (const cv::Exception&)
    {
        return false;
    }
}

std::size_t CameraCalibration::getImageCount() const
{
    return observations_.size();
}

bool CameraCalibration::setCharucoConfig(const CharucoConfig& config)
{
    if (!isValidCharucoConfig(config))
        return false;

    if (sameCharucoConfig(charucoConfig_, config))
        return true;

    charucoConfig_ = config;
    clearObservations();
    return true;
}

const CharucoConfig& CameraCalibration::getCharucoConfig() const
{
    return charucoConfig_;
}

CalibrationLoadStatus CameraCalibration::loadCalibration()
{
    MonocularCalibrationResult loadedResult;

    const CalibrationLoadStatus status =
        readCalibrationFile(calibrationFilePath(), loadedResult);

    if (status != CalibrationLoadStatus::Loaded)
    {
        calibrationResult_ = {};
        isCalibrationValid_ = false;
        return status;
    }

    calibrationResult_ = std::move(loadedResult);
    isCalibrationValid_ = true;
    return CalibrationLoadStatus::Loaded;
}

CalibrationStatus CameraCalibration::calibrate()
{
    if (observations_.size() < MIN_OBSERVATIONS)
        return CalibrationStatus::NotEnoughObservations;

    if (imageSize_.width <= 0 || imageSize_.height <= 0)
        return CalibrationStatus::CalibrationFailed;

    cv::aruco::Dictionary dictionary;
    if (!getOpenCvDictionary(charucoConfig_.dictionary, dictionary))
        return CalibrationStatus::CalibrationFailed;

    try
    {
        const cv::aruco::CharucoBoard board(
            cv::Size(charucoConfig_.columns, charucoConfig_.rows),
            charucoConfig_.squareSize,
            charucoConfig_.markerSize,
            dictionary
            );

        std::vector<std::vector<cv::Point3f>> allObjectPoints;
        std::vector<std::vector<cv::Point2f>> allImagePoints;

        allObjectPoints.reserve(observations_.size());
        allImagePoints.reserve(observations_.size());

        for (const CharucoObservation& observation : observations_)
        {
            std::vector<cv::Point3f> objectPoints;
            std::vector<cv::Point2f> imagePoints;

            board.matchImagePoints(
                observation.corners,
                observation.ids,
                objectPoints,
                imagePoints
                );

            if (objectPoints.size() != observation.corners.size() ||
                imagePoints.size() != observation.corners.size() ||
                objectPoints.empty())
            {
                return CalibrationStatus::CalibrationFailed;
            }

            allObjectPoints.push_back(std::move(objectPoints));
            allImagePoints.push_back(std::move(imagePoints));
        }

        MonocularCalibrationResult candidate;
        candidate.imageSize = imageSize_;

        candidate.rmsError = cv::calibrateCamera(
            allObjectPoints,
            allImagePoints,
            candidate.imageSize,
            candidate.cameraMatrix,
            candidate.distCoeffs,
            cv::noArray(),
            cv::noArray()
            );

        if (!isValidCalibrationResult(candidate))
            return CalibrationStatus::CalibrationFailed;

        if (!saveCalibration(candidate))
            return CalibrationStatus::WriteError;

        calibrationResult_ = std::move(candidate);
        isCalibrationValid_ = true;

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

bool CameraCalibration::clearCalibration()
{
    std::error_code error;

    const std::filesystem::path path = calibrationFilePath();
    const bool exists = std::filesystem::exists(path, error);

    if (error)
        return false;

    if (exists)
    {
        std::filesystem::remove(path, error);
        if (error)
            return false;
    }

    calibrationResult_ = {};
    isCalibrationValid_ = false;

    return true;
}

void CameraCalibration::clearObservations()
{
    observations_.clear();
    imageSize_ = cv::Size(0, 0);
}

bool CameraCalibration::saveCalibration(const MonocularCalibrationResult& result)
{
    if (!isValidCalibrationResult(result))
        return false;

    std::error_code error;
    std::filesystem::create_directories(calibrationDirectory_, error);

    if (error)
        return false;

    const std::filesystem::path finalPath = calibrationFilePath();
    std::filesystem::path tempPath = finalPath;
    tempPath += ".tmp";

    std::filesystem::remove(tempPath, error);
    error.clear();

    try
    {
        cv::FileStorage storage(
            tempPath.string(),
            cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON
            );

        if (!storage.isOpened())
            return false;

        storage << "cameraMatrix" << result.cameraMatrix;
        storage << "distCoeffs" << result.distCoeffs;
        storage << "imageWidth" << result.imageSize.width;
        storage << "imageHeight" << result.imageSize.height;
        storage << "rmsError" << result.rmsError;

        storage.release();
    }
    catch (const cv::Exception&)
    {
        std::filesystem::remove(tempPath, error);
        return false;
    }

    MonocularCalibrationResult verification;
    if (readCalibrationFile(tempPath, verification) != CalibrationLoadStatus::Loaded)
    {
        std::filesystem::remove(tempPath, error);
        return false;
    }

    std::filesystem::rename(tempPath, finalPath, error);

    if (error)
    {
        std::filesystem::remove(tempPath, error);
        return false;
    }

    return true;
}

std::filesystem::path CameraCalibration::calibrationFilePath() const
{
    return calibrationDirectory_ /
           ("camera" + std::to_string(cameraId_) + ".json");
}
