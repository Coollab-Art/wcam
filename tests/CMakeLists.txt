cmake_minimum_required(VERSION 3.11)
project(webcam_info-tests)

# ---Create executable---
add_executable(${PROJECT_NAME} tests.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_11)

# ---Set warning level---
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -pedantic-errors -Wconversion -Wsign-conversion -Wimplicit-fallthrough)
endif()

# ---Maybe enable warnings as errors---
if(WARNINGS_AS_ERRORS_FOR_WEBCAM_INFO)
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /WX)
    else()
        target_compile_options(${PROJECT_NAME} PRIVATE -Werror)
    endif()
endif()

# ---Include our library---
add_subdirectory(.. ${CMAKE_CURRENT_SOURCE_DIR}/build/webcam_info)
target_link_libraries(${PROJECT_NAME} PRIVATE webcam_info::webcam_info)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE mf mfplat mfreadwrite mfuuid)
endif()

# ---Add quick_imgui---
include(FetchContent)
FetchContent_Declare(
    quick_imgui
    GIT_REPOSITORY https://github.com/CoolLibs/quick_imgui
    GIT_TAG 1a2c38b9976d81889799fe1fb3e0c993dd787a70
)
FetchContent_MakeAvailable(quick_imgui)
target_include_directories(webcam_info SYSTEM PRIVATE ${quick_imgui_SOURCE_DIR}/lib) # Give our library access to Dear ImGui
target_link_libraries(${PROJECT_NAME} PRIVATE quick_imgui::quick_imgui)

# ---Ignore .vscode/settings.json in Git---
find_package(Git QUIET)

if(GIT_FOUND)
    get_filename_component(PARENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)

    if(EXISTS "${PARENT_DIR}/.git")
        execute_process(COMMAND ${GIT_EXECUTABLE} update-index --assume-unchanged .vscode/settings.json
            WORKING_DIRECTORY ${PARENT_DIR}
            RESULT_VARIABLE ERRORS)

        if(NOT ERRORS EQUAL "0")
            message("Git assume-unchanged failed: ${ERRORS}")
        endif()
    else()
        message("No Git repository found.")
    endif()
else()
    message("Git executable not found.")
endif()
