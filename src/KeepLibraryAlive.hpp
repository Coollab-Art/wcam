#pragma once
#include <memory>

namespace wcam {

namespace internal {
class Manager;
}

class KeepLibraryAlive {
public:
    KeepLibraryAlive();

private:
    std::shared_ptr<internal::Manager> _manager;
};

} // namespace wcam