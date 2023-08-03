#include "webcam_info/webcam_info.hpp"
#include <AVFoundation/AVFoundation.h>
#include <string>
#include <vector>

namespace webcam_info {

enum class pixel_format { unknown, yuyv, mjpeg };

struct info {
  info(const std::string &name, int width, int height,
       pixel_format format = pixel_format::unknown)
      : name(name), width(width), height(height), format(format){};
  std::string name{};
  int width{};
  int height{};
  pixel_format format{pixel_format::unknown};
};

static auto to_string(webcam_info::pixel_format format) -> std::string {
  switch (format) {
  case webcam_info::pixel_format::yuyv:
    return "yuyv";

  case webcam_info::pixel_format::mjpeg:
    return "mjpeg";

  default:
    return "unknown";
  }
}

auto get_all_webcams() -> std::vector<info> {
  std::vector<info> list_webcams_infos{};

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
    // std::cout << "Device Name: " << deviceName << std::endl;

    for (AVCaptureDeviceFormat *format in device.formats) {
      CMVideoDimensions dimensions =
          CMVideoFormatDescriptionGetDimensions(format.formatDescription);
      //   std::cout << "Video format: " << dimensions.width << "x"
      // << dimensions.height << std::endl;
      list_webcams_infos.emplace_back(deviceName, dimensions.width,
                                      dimensions.height,
                                      webcam_info::pixel_format::unknown);
    }
  }
  return list_webcams_infos;
}
} // namespace webcam_info