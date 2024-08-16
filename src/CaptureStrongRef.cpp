#include "CaptureStrongRef.hpp"
#include "internal/CapturesManager.hpp"

namespace wcam {

[[nodiscard]] auto CaptureStrongRef::image() -> MaybeImage
{
    return internal::captures_manager().image(_ptr);
}

} // namespace wcam