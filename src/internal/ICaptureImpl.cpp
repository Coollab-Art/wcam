#include "ICaptureImpl.hpp"

namespace wcam::internal {

auto ICaptureImpl::image() -> MaybeImage
{
    std::lock_guard lock{_mutex};
    return _image;
}

void ICaptureImpl::set_image(MaybeImage image)
{
    std::unique_lock lock{_mutex};
    _image = std::move(image);
}

} // namespace wcam::internal