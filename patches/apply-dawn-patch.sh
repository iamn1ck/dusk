#!/usr/bin/env bash
# apply-dawn-patch.sh — applied via FetchContent PATCH_COMMAND
# Injects Android XR instance extension injection into Dawn's BackendVk.cpp
# and DeviceVk.cpp.
# Must be run from the dawn source root directory.
set -e
BACKENDVK="src/dawn/native/vulkan/BackendVk.cpp"
DEVICEVK="src/dawn/native/vulkan/DeviceVk.cpp"

if grep -q "kXrRequiredExts" "$BACKENDVK" 2>/dev/null && grep -q "kXrRequiredDeviceExts" "$DEVICEVK" 2>/dev/null; then
    echo "Dawn Android XR patches already applied, skipping."
    exit 0
fi

echo "Applying Dawn Android XR extensions patch..."

# The BackendVk injection goes after the closing brace of the extensionNames loop.
# The DeviceVk injection goes after the closing brace of the extensionNames loop.
python3 - <<'PYEOF'
import sys

# 1. Patch BackendVk.cpp
backend_file = "src/dawn/native/vulkan/BackendVk.cpp"
backend_injection = r"""
#if DAWN_PLATFORM_IS(ANDROID)
    // The Meta OVR Vulkan runtime (ovrVulkanLoader) explicitly checks for these extensions by
    // name even though they are promoted to Vulkan 1.1 core. Request them if available so that
    // xrGetVulkanGraphicsDeviceKHR succeeds and OpenXR session initialization doesn't abort.
    {
        static const char* kXrRequiredExts[] = {
            "VK_KHR_get_physical_device_properties2",
            "VK_KHR_external_memory_capabilities",
            "VK_KHR_external_fence_capabilities",
            "VK_KHR_external_semaphore_capabilities",
        };
        uint32_t availCount = 0;
        mFunctions.EnumerateInstanceExtensionProperties(nullptr, &availCount, nullptr);
        std::vector<VkExtensionProperties> avail(availCount);
        mFunctions.EnumerateInstanceExtensionProperties(nullptr, &availCount, avail.data());
        for (const char* xrExt : kXrRequiredExts) {
            bool alreadyPresent = false;
            for (const char* n : extensionNames)
                if (strcmp(n, xrExt) == 0) { alreadyPresent = true; break; }
            if (alreadyPresent) continue;
            for (const auto& ep : avail)
                if (strcmp(ep.extensionName, xrExt) == 0) { extensionNames.push_back(xrExt); break; }
        }
    }
#endif  // DAWN_PLATFORM_IS(ANDROID)
"""

with open(backend_file, 'r') as f:
    lines = f.readlines()

insert_after = -1
in_loop = False
for i, line in enumerate(lines):
    if 'extensionNames.push_back(info.name)' in line:
        in_loop = True
    if in_loop and line.strip() == '}':
        insert_after = i
        break

if insert_after < 0:
    print("ERROR: Could not find insertion point in BackendVk.cpp", file=sys.stderr)
    sys.exit(1)

if "kXrRequiredExts" not in "".join(lines):
    lines.insert(insert_after + 1, backend_injection)
    with open(backend_file, 'w') as f:
        f.writelines(lines)
    print(f"Injected Android XR extensions block into BackendVk.cpp after line {insert_after + 1}")
else:
    print("BackendVk.cpp patch already present.")


# 2. Patch DeviceVk.cpp
device_file = "src/dawn/native/vulkan/DeviceVk.cpp"
device_injection = r"""
#if DAWN_PLATFORM_IS(ANDROID)
    // Inject OpenXR required device extensions
    {
        static const char* kXrRequiredDeviceExts[] = {
            "VK_KHR_swapchain",
            "VK_KHR_get_memory_requirements2",
            "VK_KHR_dedicated_allocation",
            "VK_KHR_external_memory",
            "VK_ANDROID_external_memory_android_hardware_buffer",
            "VK_KHR_external_semaphore",
            "VK_KHR_external_fence",
            "VK_KHR_bind_memory2",
            "VK_KHR_image_format_list",
            "VK_KHR_sampler_ycbcr_conversion",
        };
        uint32_t availCount = 0;
        const VulkanFunctions& funcs = ToBackend(GetPhysicalDevice())->GetVulkanInstance()->GetFunctions();
        funcs.EnumerateDeviceExtensionProperties(vkPhysicalDevice, nullptr, &availCount, nullptr);
        std::vector<VkExtensionProperties> avail(availCount);
        funcs.EnumerateDeviceExtensionProperties(vkPhysicalDevice, nullptr, &availCount, avail.data());
        for (const char* xrExt : kXrRequiredDeviceExts) {
            bool alreadyPresent = false;
            for (const char* n : extensionNames)
                if (strcmp(n, xrExt) == 0) { alreadyPresent = true; break; }
            if (alreadyPresent) continue;
            for (const auto& ep : avail) {
                if (strcmp(ep.extensionName, xrExt) == 0) {
                    extensionNames.push_back(xrExt);
                    break;
                }
            }
        }
    }
#endif  // DAWN_PLATFORM_IS(ANDROID)
"""

with open(device_file, 'r') as f:
    lines = f.readlines()

insert_after = -1
in_loop = False
for i, line in enumerate(lines):
    if 'extensionNames.push_back(info.name)' in line:
        in_loop = True
    if in_loop and line.strip() == '}':
        insert_after = i
        break

if insert_after < 0:
    print("ERROR: Could not find insertion point in DeviceVk.cpp", file=sys.stderr)
    sys.exit(1)

if "kXrRequiredDeviceExts" not in "".join(lines):
    lines.insert(insert_after + 1, device_injection)
    with open(device_file, 'w') as f:
        f.writelines(lines)
    print(f"Injected Android XR extensions block into DeviceVk.cpp after line {insert_after + 1}")
else:
    print("DeviceVk.cpp patch already present.")

PYEOF

echo "Dawn Android XR patches applied successfully."
