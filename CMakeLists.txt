cmake_minimum_required(VERSION 3.16)

project(Telependulo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED COMPONENTS core videoio imgcodecs)

find_path(GPIOD_INCLUDE_DIR gpiod.h REQUIRED)
find_library(GPIOD_LIBRARY NAMES gpiod REQUIRED)

add_executable(Telependulo
    main.cpp

    communication/httpserver.cpp
    communication/httpserver.h

    web/pages.cpp
    web/pages.h

    camera/camera.h
    camera/camerausb.cpp
    camera/camerausb.h
    camera/cameraip.cpp
    camera/cameraip.h

    lighting/lighting.h
    lighting/lightinggpio.cpp
    lighting/lightinggpio.h
)

target_include_directories(
    Telependulo
    PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${GPIOD_INCLUDE_DIR}
)

target_link_libraries(
    Telependulo
    PRIVATE
    ${OpenCV_LIBS}
    ${GPIOD_LIBRARY}
)