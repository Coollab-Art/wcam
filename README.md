# wcam

## Including

To add this library to your project, simply add these two lines to your *CMakeLists.txt*:
```cmake
add_subdirectory(path/to/wcam)
target_link_libraries(${PROJECT_NAME} PRIVATE wcam::wcam)
```

Then include it as:
```cpp
#include <wcam/wcam.hpp>
```
<br/>

**IMPORTANT**: On MacOS, in order for your application to be able to access the webcam (when installing your app on end-users machines), you need to add this in your *Info.plist* file:
```xml
<key>NSCameraUsageDescription</key>
<string>This app requires camera access because [your reason here].</string>
```
See [the documentation about this](https://developer.apple.com/documentation/bundleresources/information_property_list/protected_resources/requesting_authorization_for_media_capture_on_macos?language=objc).

## Using

Def your image type. You need to at least implement set_rgb_data. Implem the other if you have something more perf to do than just convert to rgb and then call your rgb func. You might at least want to implem bgr (on windows you will often receive BGR, never RGB directly)

## Running the tests

Simply use "tests/CMakeLists.txt" to generate a project, then run it.<br/>
If you are using VSCode and the CMake extension, this project already contains a *.vscode/settings.json* that will use the right CMakeLists.txt automatically.
