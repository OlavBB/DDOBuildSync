cmake_minimum_required(VERSION 3.20)
project(DDOBuildSync LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch nlohmann/json
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

set(SOURCES
    src/main.cpp
    src/main_window.cpp
    src/git_manager.cpp
    src/config.cpp
    src/utils.cpp
)

set(HEADERS
    src/main_window.h
    src/git_manager.h
    src/config.h
    src/utils.h
)

add_executable(DDOBuildSync WIN32
    ${SOURCES}
    ${HEADERS}
    resources/app.rc
)

target_compile_definitions(DDOBuildSync PRIVATE UNICODE _UNICODE)
target_include_directories(DDOBuildSync PRIVATE src)
target_link_libraries(DDOBuildSync PRIVATE
    nlohmann_json::nlohmann_json
    user32
    kernel32
    advapi32
    comctl32
    shell32
    comdlg32
    ole32
)

# Embed manifest via linker (not RC file, to avoid duplicate resource errors)
set_target_properties(DDOBuildSync PROPERTIES
    LINK_FLAGS "/MANIFEST:EMBED /MANIFESTINPUT:\"${CMAKE_SOURCE_DIR}/resources/app.manifest\""
)

# Copy default config next to executable (only if not already present)
add_custom_command(TARGET DDOBuildSync POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DSRC="${CMAKE_SOURCE_DIR}/config/default_config.json"
        -DDST="$<TARGET_FILE_DIR:DDOBuildSync>/default_config.json"
        -P "${CMAKE_SOURCE_DIR}/cmake/copy_if_not_exists.cmake"
)
