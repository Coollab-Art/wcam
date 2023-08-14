#include <AVFoundation/AVFoundation.h>
#include <CoreMedia/CMFormatDescription.h>
#include <string>
#include <vector>
#include <webcam_info/webcam_info.hpp>

namespace webcam_info {

auto grab_all_webcams_infos_impl() -> std::vector<Info> {
    std::vector<Info> list_webcams_infos{};

    @autoreleasepool
    {
        AVCaptureDeviceDiscoverySession *discoverySession =
            [AVCaptureDeviceDiscoverySession
                discoverySessionWithDeviceTypes:@[
                    AVCaptureDeviceTypeBuiltInWideAngleCamera,
                    AVCaptureDeviceTypeBuiltInUltraWideCamera,
                    AVCaptureDeviceTypeBuiltInTelephotoCamera,
                    AVCaptureDeviceTypeBuiltInDualCamera,
                    AVCaptureDeviceTypeBuiltInDualWideCamera,
                    AVCaptureDeviceTypeBuiltInTripleCamera,
                    AVCaptureDeviceTypeContinuityCamera
                ]
                mediaType:AVMediaTypeVideo
                position:AVCaptureDevicePositionUnspecified];

        if (!discoverySession)
            return list_webcams_infos;

        NSArray *devices = discoverySession.devices;

        for (AVCaptureDevice *device in devices) {
                std::string deviceName = [[device localizedName] UTF8String];
                std::vector<webcam_info::Resolution> list_resolution{};

                for (AVCaptureDeviceFormat *format in device.formats) {
                    CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
                    list_resolution.push_back({dimensions.width, dimensions.height});
                }
                list_webcams_infos.push_back({deviceName, list_resolution});
        }
    }
    return list_webcams_infos;
}

} // namespace webcam_info