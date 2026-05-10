#ifdef TARGET_PC

#include "dusk/vr.hpp"

// Forward-declare the one function we need from the aurora internal layer.
// We cannot include openxr_integration.hpp here because it transitively pulls
// in <webgpu/webgpu_cpp.h>, which is not on the game target's include path.
namespace aurora::openxr {
    bool get_head_pose(float& ox, float& oy, float& oz, float& ow);
}

namespace dusk::vr {

bool get_head_pose(float& ox, float& oy, float& oz, float& ow) {
    return aurora::openxr::get_head_pose(ox, oy, oz, ow);
}

} // namespace dusk::vr

#endif // TARGET_PC

