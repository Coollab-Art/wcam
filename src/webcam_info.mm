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

#include <CoreMedia/CMFormatDescription.h>
// #include <CoreMedia/CMVideoFormatDescription.h>
#include <CoreMediaIO/CMIOHardware.h>

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

  CMIOObjectID deviceID = kCMIOObjectPropertyScopeGlobal;
  CMIOObjectPropertyAddress propAddress;
  propAddress.mSelector = kCMIOHardwarePropertyDevices;
  propAddress.mScope = kCMIOObjectPropertyScopeGlobal;

  CMItemCount deviceCount = 0;
  OSStatus status = CMIOObjectGetPropertyDataSize(
      kCMIOObjectSystemObject, &propAddress, 0, nullptr, &deviceCount);

  if (status == noErr && deviceCount > 0) {
    CMIOObjectID *deviceIDs = new CMIOObjectID[deviceCount];
    status = CMIOObjectGetPropertyData(
        kCMIOObjectSystemObject, &propAddress, 0, nullptr,
        deviceCount * sizeof(CMIOObjectID), &deviceCount, deviceIDs);

    if (status == noErr) {
      for (CMItemCount i = 0; i < deviceCount; i++) {
        list_webcams_infos.push_back(
            get_webcam_info_from_device_id(deviceIDs[i]));
      }
    }

    delete[] deviceIDs;
  }
  return list_webcams_infos;
}