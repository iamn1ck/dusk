#pragma once

#ifdef TARGET_PC

namespace dusk::vr {

// Returns the HMD head orientation as a quaternion (x, y, z, w) from the most
// recent OpenXR xrLocateViews call. Returns false when OpenXR is not active or
// the pose has not yet been located.
bool get_head_pose(float& ox, float& oy, float& oz, float& ow);

}  // namespace dusk::vr

#endif  // TARGET_PC
