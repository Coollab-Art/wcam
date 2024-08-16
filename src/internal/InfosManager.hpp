#pragma once
#include <vector>
#include "../Info.hpp"

namespace wcam::internal {

class InfosManager {
public:
    auto infos() -> std::vector<Info>;

private:
    std::vector<Info> _infos{};
};

inline auto infos_manager() -> InfosManager&
{
    static auto instance = InfosManager{};
    return instance;
}

} // namespace wcam::internal