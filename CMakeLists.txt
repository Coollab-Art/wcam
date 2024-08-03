cmake_minimum_required(VERSION 3.13)

set(WARNINGS_AS_ERRORS_FOR_WCAM OFF CACHE BOOL "ON iff you want to treat warnings as errors")

add_library(wcam)
add_library(wcam::wcam ALIAS wcam)
target_compile_features(wcam PUBLIC cxx_std_20)

# ---Add source files---
if(WARNINGS_AS_ERRORS_FOR_WCAM)
    target_include_directories(wcam PUBLIC include)
else()
    target_include_directories(wcam SYSTEM PUBLIC include)
endif()

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS src/*.cpp)
target_sources(wcam PRIVATE ${SRC_FILES})

# ---Add img library---
add_subdirectory(lib/img)
target_link_libraries(wcam PUBLIC img::img)

# ---Set warning level---
if(MSVC)
    target_compile_options(wcam PRIVATE /W4)
else()
    target_compile_options(wcam PRIVATE -Wall -Wextra -Wpedantic -pedantic-errors -Wconversion -Wsign-conversion -Wimplicit-fallthrough)
endif()

# ---Maybe enable warnings as errors---
if(WARNINGS_AS_ERRORS_FOR_WCAM)
    if(MSVC)
        target_compile_options(wcam PRIVATE /WX)
    else()
        target_compile_options(wcam PRIVATE -Werror)
    endif()
endif()

# ---Platform-specific setup---
if(WIN32)
    target_link_libraries(wcam PRIVATE strmiids)
elseif(APPLE)
    target_link_libraries(wcam PRIVATE "-framework AVFoundation")
    target_link_libraries(wcam PRIVATE "-framework CoreMedia")

    # target_link_libraries(wcam PRIVATE "-framework IOKit")
    file(GLOB_RECURSE SRC_FILES_MACOS CONFIGURE_DEPENDS src/*.mm)
    target_sources(wcam PRIVATE ${SRC_FILES_MACOS})

    if(NOT XCODE)
        set_source_files_properties(${SRC_FILES_MACOS} PROPERTIES COMPILE_FLAGS -fobjc-arc)
    endif()
endif()