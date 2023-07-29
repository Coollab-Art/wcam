# webcam_info

## Including

To add this library to your project, simply add these two lines to your *CMakeLists.txt*:
```cmake
add_subdirectory(path/to/webcam_info)
target_link_libraries(${PROJECT_NAME} PRIVATE webcam_info::webcam_info)
```

Then include it as:
```cpp
#include <webcam_info/webcam_info.hpp>
```

## Running the tests

Simply use "tests/CMakeLists.txt" to generate a project, then run it.<br/>
If you are using VSCode and the CMake extension, this project already contains a *.vscode/settings.json* that will use the right CMakeLists.txt automatically.
