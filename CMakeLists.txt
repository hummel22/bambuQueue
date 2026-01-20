cmake_minimum_required(VERSION 3.16)
project(BambuQueue LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

add_executable(bambu_queue
    src/main.cpp
    src/app/AppCore.cpp
)

target_link_libraries(bambu_queue PRIVATE ${wxWidgets_LIBRARIES})
target_include_directories(bambu_queue PRIVATE src)
