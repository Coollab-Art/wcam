#include <AVFoundation/AVFoundation.h>
#include <CoreMedia/CMFormatDescription.h>
#include <string>
#include <vector>
#include <iostream>
#include <AVFoundation/AVFoundation.h>
#include <CoreMedia/CMFormatDescription.h>
#include <CoreVideo/CVPixelBuffer.h>
#include <Cocoa/Cocoa.h>
#include <wcam/wcam.hpp>

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


@interface FrameCaptureDelegate : NSObject<AVCaptureVideoDataOutputSampleBufferDelegate>
{
@public
    std::vector<webcam_info::FrameInfo>* frames;
}
@property (nonatomic, strong) NSImageView *imageView;
@property (nonatomic) CGSize frameSize;
@end

@implementation FrameCaptureDelegate

- (void)captureOutput:(AVCaptureOutput *)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection *)connection {
    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);

    // Convertir l'image buffer en CIImage
    CIImage *ciImage = [CIImage imageWithCVImageBuffer:imageBuffer];

    // Convertir CIImage en NSImage
    NSCIImageRep *imageRep = [NSCIImageRep imageRepWithCIImage:ciImage];
    NSImage *nsImage = [[NSImage alloc] initWithSize:imageRep.size];
    [nsImage addRepresentation:imageRep];

    // Extraire les données des pixels
    size_t bufferWidth = CVPixelBufferGetWidth(imageBuffer);
    size_t bufferHeight = CVPixelBufferGetHeight(imageBuffer);
    CVPixelBufferLockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);

    OSType pixelFormat = CVPixelBufferGetPixelFormatType(imageBuffer);
    NSString *pixelFormatHexString = [NSString stringWithFormat:@"%08X", pixelFormat];
    NSLog(@"Pixel format hex: %@", pixelFormatHexString);

    std::vector<uint8_t> yData, cbCrData; // Vecteurs pour stocker les données Y et CbCr

    if (pixelFormat == kCVPixelFormatType_422YpCbCr8) {
        // Composante Y (Luminance)
        uint8_t *baseAddressY = (uint8_t *)CVPixelBufferGetBaseAddress(imageBuffer);
        size_t bytesPerRowY = CVPixelBufferGetBytesPerRow(imageBuffer);
        size_t dataSizeY = bytesPerRowY * bufferHeight;
        yData.assign(baseAddressY, baseAddressY + dataSizeY); // Copier les données Y dans le vecteur

        // Composante CbCr (Chrominance bleue et rouge intercalées)
        // Note : Pour le format 422, les données CbCr sont intercalées avec les données Y
        cbCrData.reserve(dataSizeY / 2); // Réserver la taille pour les données CbCr
        for (size_t i = 1; i < dataSizeY; i += 2) {
            cbCrData.push_back(baseAddressY[i]); // Ajouter les données Cb ou Cr à partir des données Y
        }

        NSLog(@"Y Data size: %zu, CbCr Data size: %zu", yData.size(), cbCrData.size());
    } else {
        NSLog(@"Unsupported pixel format: %u", pixelFormat);
    }

    CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);

    // Stocker les informations des frames
    self.frameSize = CVImageBufferGetEncodedSize(imageBuffer);
    NSInteger width = self.frameSize.width;
    NSInteger height = self.frameSize.height;

    webcam_info::FrameInfo frameInfo;
    frameInfo.width = (int)width;
    frameInfo.height = (int)height;
    frameInfo.yData = yData; // Stocker les données Y
    frameInfo.cbCrData = cbCrData; // Stocker les données CbCr
    frames->push_back(frameInfo);

    // Debugging: Log the size information
    NSLog(@"Frame width: %ld, height: %ld", (long)width, (long)height);

    dispatch_async(dispatch_get_main_queue(), ^{
        self.imageView.image = nsImage;
    });
}

@end






//PARTIE 3 : AFFICHAGE DE LA CAMERA DANS l'APP

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (nonatomic, strong) NSWindow *window;
@property (nonatomic, strong) FrameCaptureDelegate *frameCaptureDelegate;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1080, 480)
                                               styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable)
                                                 backing:NSBackingStoreBuffered
                                                   defer:NO];
    [self.window setTitle:@"Webcam Capture"];
    [self.window makeKeyAndOrderFront:nil];

    NSImageView *imageView = [[NSImageView alloc] initWithFrame:self.window.contentView.bounds];
    imageView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [self.window.contentView addSubview:imageView];

    self.frameCaptureDelegate = [[FrameCaptureDelegate alloc] init];
    self.frameCaptureDelegate.imageView = imageView;
    self.frameCaptureDelegate->frames = new std::vector<webcam_info::FrameInfo>;

    AVCaptureSession *session = [[AVCaptureSession alloc] init];
    AVCaptureDevice *device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];

    if (!device) {
        std::cerr << "No video device found" << std::endl;
        [NSApp terminate:nil];
    }

    NSError *error = nil;
    AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&error];
    if (error) {
        std::cerr << "Error opening video device: " << [[error localizedDescription] UTF8String] << std::endl;
        [NSApp terminate:nil];
    }

    [session addInput:input];
    
    AVCaptureVideoDataOutput *output = [[AVCaptureVideoDataOutput alloc] init];
    [output setSampleBufferDelegate:self.frameCaptureDelegate queue:dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0)];
    [session addOutput:output];
    
    [session startRunning];
}

@end


void open_webcam()
{
     @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        AppDelegate *delegate = [[AppDelegate alloc] init];
        app.delegate = delegate;
        [app run];
    }
}


} // namespace wcam::internal