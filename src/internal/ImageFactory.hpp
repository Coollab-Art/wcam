#pragma once
#include <cassert>
#include <memory>
#include "../Image.hpp"
#include "../Resolution.hpp"

namespace wcam::internal {

class IImageFactory {
public:
    IImageFactory()                                            = default;
    virtual ~IImageFactory()                                   = default;
    IImageFactory(IImageFactory const&)                        = delete;
    auto operator=(IImageFactory const&) -> IImageFactory&     = delete;
    IImageFactory(IImageFactory&&) noexcept                    = delete;
    auto operator=(IImageFactory&&) noexcept -> IImageFactory& = delete;

    virtual auto make_image() const -> std::shared_ptr<Image> = 0;
};

template<typename ImageT>
class ImageFactory : public IImageFactory {
public:
    auto make_image() const -> std::shared_ptr<Image> override
    {
        return std::make_shared<ImageT>();
    }
};

inline auto image_factory_pointer() -> std::unique_ptr<IImageFactory>&
{
    static auto instance = std::unique_ptr<IImageFactory>{};
    return instance;
}

inline auto image_factory() -> IImageFactory&
{
    assert(image_factory_pointer() && "You must call `wcam::set_image_type<YourImageType>();` once at the beginning of your application");
    return *image_factory_pointer();
}

} // namespace wcam::internal