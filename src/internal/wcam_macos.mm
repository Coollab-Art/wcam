#include <AVFoundation/AVFoundation.h>
#include <CoreMedia/CMFormatDescription.h>
#include <string>
#include <vector>
#include <iostream>
#include <AVFoundation/AVFoundation.h>
#include <CoreMedia/CMFormatDescription.h>
#include <CoreVideo/CVPixelBuffer.h>
#include "make_device_id.hpp"
#include <Cocoa/Cocoa.h>
#include <wcam/wcam.hpp>
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

#include "wcam_macos.hpp"
#import <CoreImage/CoreImage.h>



// C++ callback definition
typedef void (*FrameCapturedCallback)(wcam::internal::CaptureImpl&, CGImageRef);

@interface WebcamCapture : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>

- (instancetype)initWithCallback:(FrameCapturedCallback)callback myptr:(wcam::internal::CaptureImpl*)ptr;
- (void)startCapturing;
- (void)stopCapturing;

@end



@implementation WebcamCapture {
    AVCaptureSession *captureSession;
   wcam::internal::CaptureImpl* trucptr;
    FrameCapturedCallback callback;
}

- (instancetype)initWithCallback:(FrameCapturedCallback)frameCallback  myptr:(wcam::internal::CaptureImpl*)ptr{
    self = [super init];
    if (self) {
        callback = frameCallback;
        trucptr=ptr;
        [self setupCaptureSession];
    }
    return self;
}

- (void)setupCaptureSession {
    captureSession = [[AVCaptureSession alloc] init];
    [captureSession beginConfiguration];
    
    // Setup the webcam (video input)
    AVCaptureDevice *videoDevice = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    NSError *error = nil;
    AVCaptureDeviceInput *videoInput = [AVCaptureDeviceInput deviceInputWithDevice:videoDevice error:&error];
    
    if ([captureSession canAddInput:videoInput]) {
        [captureSession addInput:videoInput];
    } else {
        NSLog(@"Failed to add video input: %@", error);
    }
    
    // Setup video output
    AVCaptureVideoDataOutput *videoOutput = [[AVCaptureVideoDataOutput alloc] init];
    dispatch_queue_t queue = dispatch_queue_create("VideoCaptureQueue", NULL);
    [videoOutput setSampleBufferDelegate:self queue:queue];
    
    if ([captureSession canAddOutput:videoOutput]) {
        [captureSession addOutput:videoOutput];
    }
    
    [captureSession commitConfiguration];
}

- (void)startCapturing {
    [captureSession startRunning];
}

- (void)stopCapturing {
    [captureSession stopRunning];
}

// Delegate method to handle captured frames
- (void)captureOutput:(AVCaptureOutput *)output
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection {
    
    CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!pixelBuffer) {
        return;
    }

    CIImage *ciImage = [CIImage imageWithCVPixelBuffer:pixelBuffer];
    CIContext *context = [CIContext context];
    
    CGImageRef cgImage = [context createCGImage:ciImage fromRect:[ciImage extent]];
    
    // Call the C++ callback with the captured image
    if (callback) {
        callback(*trucptr, cgImage);
    }

    CGImageRelease(cgImage);
}

@end


    // C++ callback function to handle the captured image
void frameCapturedCallback(wcam::internal::CaptureImpl& Self, CGImageRef image) {
        Self.webcam_callback(image);
}

namespace wcam::internal {

auto grab_all_infos_impl() -> std::vector<Info> {
    std::vector<Info> list_webcams_infos{};

    @autoreleasepool
    {
        AVCaptureDeviceDiscoverySession *discoverySession =
            [AVCaptureDeviceDiscoverySession
                discoverySessionWithDeviceTypes:@[
                    AVCaptureDeviceTypeBuiltInWideAngleCamera
                ]
                mediaType:AVMediaTypeVideo
                position:AVCaptureDevicePositionUnspecified];

        if (!discoverySession)
            return list_webcams_infos;

        NSArray *devices = discoverySession.devices;

        for (AVCaptureDevice *device in devices) {
                std::string deviceName = [[device localizedName] UTF8String];
                std::vector<Resolution> list_resolution{};

                for (AVCaptureDeviceFormat *format in device.formats) {
                    CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
                    list_resolution.push_back({static_cast<Resolution::DataType>(dimensions.width), static_cast<Resolution::DataType>(dimensions.height)});
                }
                list_webcams_infos.push_back({deviceName,make_device_id(deviceName), list_resolution});
        }
    }
    return list_webcams_infos;
}


void CaptureImpl::webcam_callback(CGImageRef image){
    
    std::cout << "okok" << "\n";

    if (image) {
        unsigned int width = CGImageGetWidth(image);
unsigned int height = CGImageGetHeight(image);

// Get the data provider
CGDataProviderRef dataProvider = CGImageGetDataProvider(image);
CFDataRef imageData = CGDataProviderCopyData(dataProvider);

// Access the raw bytes
const UInt8 *rawData = CFDataGetBytePtr(imageData);
    auto image = image_factory().make_image();
  
    image->set_data(ImageDataView<RGBA24>{rawData, static_cast<size_t>(width*height*4), {width, height}, wcam::FirstRowIs::Top});
    // else
    // {
        // ICaptureImpl::set_image(Error_Unknown{"Unsupported pixel format"});
    // }

    ICaptureImpl::set_image(std::move(image));
    } else {
        std::cout << "Failed to capture image" << std::endl;
    }
}
static WebcamCapture *webcamCapture;

void CaptureImpl::open_webcam()
{
    //  @autoreleasepool {
        // NSApplication *app = [NSApplication sharedApplication];
        // std::cout << "ok1" << "\n";
        // // AppDelegate *delegate = [[AppDelegate alloc] init];
        // // app.delegate = delegate;
        // std::cout << "ok2" << "\n";
        // [app run];
        //  std::cout << "ok3" << "\n";
        webcamCapture = [[WebcamCapture alloc] initWithCallback:frameCapturedCallback myptr:this];
        
        // Start capturing frames
        [webcamCapture startCapturing];
        
        // std::cout << "Press Enter to stop capturing..." << std::endl;
       // std::cin.get();
        
        // // Stop capturing frames
        // [webcamCapture stopCapturing];
    // }
}

}


 // namespace wcam::internal