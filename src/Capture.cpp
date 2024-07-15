WebcamCapture::~WebcamCapture()
{
    _wants_to_stop_thread.store(true);
    _thread.join();
}

static auto create_directct_capture(size_t webcam_index, webcam_info::Resolution resolution) -> cv::VideoCapture
{
    // TODO
}

void WebcamCapture::thread_job(WebcamCapture& This, webcam_info::Resolution resolution)
{
    try
    {
        cv::VideoCapture capture = create_directct_capture(This.webcam_index(), resolution);
        cv::Mat          wip_image{};
        while (!This._wants_to_stop_thread.load() && capture.isOpened())
        {
            // TODO
            long cbBuffer = 0;
            pGrabber->GetCurrentBuffer(&cbBuffer, NULL);
            BYTE* pBuffer = new BYTE[cbBuffer];
            pGrabber->GetCurrentBuffer(&cbBuffer, (long*)pBuffer);
            //

            std::scoped_lock lock{This._mutex};
            cv::swap(This._available_image, wip_image);
            This._needs_to_create_texture_from_available_image = true;
        }
    }
    catch (...)
    {
    }
    This._has_stopped = true;
}

void WebcamCapture::update_webcam_ifn()
{
    std::scoped_lock lock{_mutex};
    if (!_needs_to_create_texture_from_available_image)
        return;

    set_texture_from_opencv_image(_texture, _available_image);
    if (DebugOptions::log_when_creating_textures())
        Log::ToUser::info("Webcam", fmt::format("Generated texture for webcam {}", _webcam_index));
    _needs_to_create_texture_from_available_image = false;
}
