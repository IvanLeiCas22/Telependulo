cmake_minimum_required(VERSION 3.16)

project(Telependulo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(Telependulo
    main.cpp
    server/httpserver.cpp
    server/httpserver.h
)

include(GNUInstallDirs)
install(TARGETS Telependulo
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
