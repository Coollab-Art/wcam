cmake_minimum_required(VERSION 3.13)

set(WARNINGS_AS_ERRORS_FOR_WEBCAM_INFO OFF CACHE BOOL "ON iff you want to treat warnings as errors")

add_library(webcam_info)
add_library(webcam_info::webcam_info ALIAS webcam_info)
target_compile_features(webcam_info PUBLIC cxx_std_20)

# ---Add source files---
if(WARNINGS_AS_ERRORS_FOR_WEBCAM_INFO)
    target_include_directories(webcam_info PUBLIC include)
else()
    target_include_directories(webcam_info SYSTEM PUBLIC include)
endif()

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS src/*.cpp)
target_sources(webcam_info PRIVATE ${SRC_FILES})

# ---Add img library---
add_subdirectory(lib/img)
target_link_libraries(webcam_info PUBLIC img::img)

# ---Set warning level---
if(MSVC)
    target_compile_options(webcam_info PRIVATE /W4)
else()
    target_compile_options(webcam_info PRIVATE -Wall -Wextra -Wpedantic -pedantic-errors -Wconversion -Wsign-conversion -Wimplicit-fallthrough)
endif()

# ---Maybe enable warnings as errors---
if(WARNINGS_AS_ERRORS_FOR_WEBCAM_INFO)
    if(MSVC)
        target_compile_options(webcam_info PRIVATE /WX)
    else()
        target_compile_options(webcam_info PRIVATE -Werror)
    endif()
endif()

# ---Platform-specific setup---
if(WIN32)
    target_link_libraries(webcam_info PRIVATE strmiids)
elseif(APPLE)
    target_link_libraries(webcam_info PRIVATE "-framework AVFoundation")
    target_link_libraries(webcam_info PRIVATE "-framework CoreMedia")

    # target_link_libraries(webcam_info PRIVATE "-framework IOKit")
    file(GLOB_RECURSE SRC_FILES_MACOS CONFIGURE_DEPENDS src/*.mm)
    target_sources(webcam_info PRIVATE ${SRC_FILES_MACOS})

    if(NOT XCODE)
        set_source_files_properties(${SRC_FILES_MACOS} PROPERTIES COMPILE_FLAGS -fobjc-arc)
    endif()
endif()