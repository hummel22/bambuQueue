cmake_minimum_required(VERSION 3.16)
project(BambuQueue LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(wxWidgets REQUIRED COMPONENTS core base xml)
find_package(SQLite3 REQUIRED)
find_package(CURL REQUIRED)
include(${wxWidgets_USE_FILE})

add_executable(bambu_queue
    src/main.cpp
    src/app/AppBootstrap.cpp
    src/app/ConfigLoader.cpp
    src/app/DatabaseManager.cpp
    src/app/FtpsClient.cpp
    src/app/ImportWatcher.cpp
    src/app/MqttClient.cpp
    src/app/PrinterCoordinator.cpp
    src/app/ThreeMfImporter.cpp
)

if(TARGET SQLite3::SQLite3)
    set(BAMBUQUEUE_SQLITE_TARGET SQLite3::SQLite3)
elseif(TARGET SQLite::SQLite3)
    set(BAMBUQUEUE_SQLITE_TARGET SQLite::SQLite3)
else()
    set(BAMBUQUEUE_SQLITE_TARGET ${SQLite3_LIBRARIES})
endif()

target_link_libraries(bambu_queue PRIVATE ${wxWidgets_LIBRARIES} ${BAMBUQUEUE_SQLITE_TARGET} CURL::libcurl)
target_include_directories(bambu_queue PRIVATE src)
