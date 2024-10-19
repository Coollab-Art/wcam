# wcam

A simple and powerful Webcam library:

- [x] cross-platform
- [x] easy to add to your project
- [x] easy to use
- [x] works well (handles webcam disconnects, and reconnects as fast as possible)

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

You need to define your own image type, and then supply it to the library by calling
```cpp
wcam::set_image_type<Image>();
```
at the beginning of your application, before you use anything from *wcam*. See the tests for more details.

You image type needs to at least implement
```cpp
void set_data(wcam::ImageDataView<wcam::RGB24> const& rgb_data) override
```
You can also implement the other overloads (from BGR, YUV, etc.) if you have something smart and performant to do. Otherwise *wcam* will just convert the data to RGB and then call the RGB overload.<br/>
You might want to at least implement BGR (on windows you will often receive BGR, never RGB directly).

## Running the tests

Simply use "tests/CMakeLists.txt" to generate a project, then run it.<br/>
If you are using VSCode and the CMake extension, this project already contains a *.vscode/settings.json* that will use the right CMakeLists.txt automatically.
