#include "KeepLibraryAlive.hpp"
#include <memory>
#include "internal/Manager.hpp"

namespace wcam {

static auto manager_weak_ptr() -> std::weak_ptr<internal::Manager>&
{
    static auto weak_ptr = std::weak_ptr<internal::Manager>{};
    return weak_ptr;
}

static auto manager_shared_ptr() -> std::shared_ptr<internal::Manager>
{
    {
        auto const shared_ptr = manager_weak_ptr().lock();
        if (shared_ptr) // Library is alive, return a pointer to the current Manager
            return shared_ptr;
    }
    // Library is not alive, create a new manager
    auto const shared_ptr = std::make_shared<internal::Manager>();
    manager_weak_ptr()    = shared_ptr;
    return shared_ptr;
}

KeepLibraryAlive::KeepLibraryAlive()
    : _manager{manager_shared_ptr()}
{
}

namespace internal {
auto manager_unchecked() -> std::shared_ptr<Manager>
{
    return manager_weak_ptr().lock();
}
auto manager() -> std::shared_ptr<Manager>
{
    auto const manager = manager_unchecked();
    if (!manager)
    {
        assert(false && "You need to have an instance of wcam::KeepLibraryAlive alive in order to call any function from wcam. See the documentation of wcam::KeepLibraryAlive for more info.");
        return std::make_shared<internal::Manager>(); // We still return a manager, so that the library doesn't crash. It is nonetheless an error in the usage of the library, because it will become very slow (creating and destroying a thread each time you call a function of the library + it's not guaranteed that the thread will have time to do its job and grab the info / init the captures)
    }
    return manager;
}
} // namespace internal

} // namespace wcam