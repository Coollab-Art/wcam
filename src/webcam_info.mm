#include <AVFoundation/AVFoundation.h>
#include <CoreMedia/CMFormatDescription.h>
// #include <CoreMedia/CMVideoFormatDescription.h>
#include <string>
#include <vector>
#include <webcam_info/webcam_info.hpp>

namespace webcam_info {

static auto grab_all_webcams_infos_impl() -> std::vector<Info> {
  std::vector<Info> list_webcams_infos{};

  AVCaptureDeviceDiscoverySession *discoverySession =
      [AVCaptureDeviceDiscoverySession
          discoverySessionWithDeviceTypes:@[
            AVCaptureDeviceTypeBuiltInWideAngleCamera
          ]
                                mediaType:AVMediaTypeVideo
                                 position:AVCaptureDevicePositionUnspecified];

  NSArray *devices = discoverySession.devices;

  for (AVCaptureDevice *device in devices) {
    std::string deviceName = [device.localizedName UTF8String];
    std::vector<webcam_info::resolution> list_resolution{};
    // std::cout << "Device Name: " << deviceName << std::endl;

    for (AVCaptureDeviceFormat *format in device.formats) {
      CMVideoDimensions dimensions =
          CMVideoFormatDescriptionGetDimensions(format.formatDescription);
      //   std::cout << "Video format: " << dimensions.width << "x"
      // << dimensions.height << std::endl;
      list_resolution.push_back({dimensions.width, dimensions.height});
    }
    list_webcams_infos.push_back(webcam_info::info{
        deviceName, list_resolution, webcam_info::pixel_format::unknown});
  }
  return list_webcams_infos;
}
} // namespace webcam_info