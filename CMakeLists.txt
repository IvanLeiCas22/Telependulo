cmake_minimum_required(VERSION 3.16)

project(Telependulo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED COMPONENTS core videoio imgcodecs)

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
)

target_include_directories(Telependulo PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(Telependulo PRIVATE ${OpenCV_LIBS})

include(GNUInstallDirs)
install(TARGETS Telependulo
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
