cmake_minimum_required(VERSION 3.18)

project(Telependulo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED COMPONENTS core videoio imgcodecs objdetect calib3d)

find_path(GPIOD_INCLUDE_DIR gpiod.h REQUIRED)
find_library(GPIOD_LIBRARY NAMES gpiod REQUIRED)

add_executable(Telependulo
    main.cpp

    camera/camera.h
    camera/camerausb.cpp
    camera/camerausb.h

    communication/httpserver.cpp
    communication/httpserver.h

    lighting/lighting.h
    lighting/lightinggpio.cpp
    lighting/lightinggpio.h

    web/pages.cpp
    web/pages.h
    web/calibrationpage.cpp
    web/calibrationpage.h
    calibration/cameracalibration.h calibration/cameracalibration.cpp
)

target_include_directories(
    Telependulo
    PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${GPIOD_INCLUDE_DIR}
)

target_compile_definitions(
    Telependulo
    PRIVATE
    TELEPENDULO_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(
    Telependulo
    PRIVATE
    ${OpenCV_LIBS}
    ${GPIOD_LIBRARY}
)

include(GNUInstallDirs)

install(TARGETS Telependulo
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
