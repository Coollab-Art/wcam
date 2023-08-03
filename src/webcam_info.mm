#include <string>
#include <vector>

namespace webcam_info {

enum class pixel_format { unknown, yuyv, mjpeg };

struct info {
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
} // namespace webcam_info

#include <AVFoundation/AVFoundation.h>

auto get_webcam_info_from_device_id(CMIOObjectID deviceID)
    -> webcam_info::info {
  webcam_info::info webcam_infos{};
  CMIOObjectPropertyAddress propAddress;
  propAddress.mSelector = kCMIOStreamPropertyFormatDescriptions;
  propAddress.mScope = kCMIODevicePropertyScopeInput;

  CMFormatDescriptionRef formatDesc = nullptr;
  UInt32 dataSize = sizeof(CMFormatDescriptionRef);
  OSStatus status = CMIOObjectGetPropertyData(
      deviceID, &propAddress, 0, nullptr, dataSize, &dataSize, &formatDesc);

  if (status == noErr && formatDesc != nullptr) {
    CMVideoFormatDescriptionRef videoDesc =
        (CMVideoFormatDescriptionRef)formatDesc;

    CMIOObjectPropertyAddress propDeviceNameAddress;
    propDeviceNameAddress.mSelector = kCMIODevicePropertyDeviceName;
    propDeviceNameAddress.mScope = kCMIODevicePropertyScopeInput;

    CFStringRef deviceName = nullptr;
    dataSize = sizeof(CFStringRef);
    status =
        CMIOObjectGetPropertyData(deviceID, &propDeviceNameAddress, 0, nullptr,
                                  dataSize, &dataSize, &deviceName);

    if (status == noErr && deviceName != nullptr) {
      char nameBuf[256];
      CFStringGetCString(deviceName, nameBuf, sizeof(nameBuf),
                         kCFStringEncodingUTF8);
      webcam_infos.name = std::string(nameBuf);
      CFRelease(deviceName);
    }

    webcam_infos.width = CMVideoFormatDescriptionGetDimensions(videoDesc).width;
    webcam_infos.height =
        CMVideoFormatDescriptionGetDimensions(videoDesc).height;

    CFRelease(formatDesc);
  }
}

auto webcam_info::get_all_webcams() -> std::vector<info> {
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
                                      dimensions.height);
    }
  }
  return list_webcams_infos;
}