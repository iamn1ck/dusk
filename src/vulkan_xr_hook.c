#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <vulkan/vulkan.h>

// Forward-declarations of functions we want to hook
VkResult vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance);
VkResult vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice);
PFN_vkVoidFunction vkGetInstanceProcAddr(VkInstance instance, const char* pName);

// Intercept vkCreateInstance
VkResult vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance) {
    PFN_vkCreateInstance real_vkCreateInstance = (PFN_vkCreateInstance)dlsym(RTLD_NEXT, "vkCreateInstance");
    if (!real_vkCreateInstance) {
        fprintf(stderr, "[vulkan_xr_hook] Failed to find real vkCreateInstance via RTLD_NEXT!\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t ext_count = pCreateInfo->enabledExtensionCount;
    const char** exts = malloc(sizeof(char*) * (ext_count + 16));
    for (uint32_t i = 0; i < ext_count; ++i) {
        exts[i] = pCreateInfo->ppEnabledExtensionNames[i];
    }

    const char* xr_exts[] = {
        "VK_KHR_external_memory_capabilities",
        "VK_KHR_get_physical_device_properties2",
        "VK_KHR_external_fence_capabilities",
        "VK_KHR_external_semaphore_capabilities",
        "VK_KHR_surface"
    };

    uint32_t added = 0;
    for (int j = 0; j < 5; ++j) {
        int found = 0;
        for (uint32_t i = 0; i < ext_count; ++i) {
            if (strcmp(exts[i], xr_exts[j]) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            exts[ext_count + added] = xr_exts[j];
            added++;
        }
    }

    VkInstanceCreateInfo newCreateInfo = *pCreateInfo;
    newCreateInfo.enabledExtensionCount = ext_count + added;
    newCreateInfo.ppEnabledExtensionNames = exts;

    // Force Vulkan 1.1 API version if the application requested 1.0, to ensure compatibility with extensions
    VkApplicationInfo newAppInfo;
    if (pCreateInfo->pApplicationInfo) {
        newAppInfo = *(pCreateInfo->pApplicationInfo);
        if (newAppInfo.apiVersion < VK_API_VERSION_1_1) {
            newAppInfo.apiVersion = VK_API_VERSION_1_1;
        }
        newCreateInfo.pApplicationInfo = &newAppInfo;
    }

    fprintf(stderr, "[vulkan_xr_hook] vkCreateInstance intercepted: added %u extensions (Total: %u)\n", added, ext_count + added);
    for (uint32_t i = 0; i < ext_count + added; ++i) {
        fprintf(stderr, "  %s\n", exts[i]);
    }

    VkResult res = real_vkCreateInstance(&newCreateInfo, pAllocator, pInstance);
    free(exts);
    return res;
}

// Intercept vkCreateDevice
VkResult vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) {
    PFN_vkCreateDevice real_vkCreateDevice = (PFN_vkCreateDevice)dlsym(RTLD_NEXT, "vkCreateDevice");
    if (!real_vkCreateDevice) {
        fprintf(stderr, "[vulkan_xr_hook] Failed to find real vkCreateDevice via RTLD_NEXT!\n");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t ext_count = pCreateInfo->enabledExtensionCount;
    const char** exts = malloc(sizeof(char*) * (ext_count + 16));
    for (uint32_t i = 0; i < ext_count; ++i) {
        exts[i] = pCreateInfo->ppEnabledExtensionNames[i];
    }

    const char* xr_exts[] = {
        "VK_KHR_external_memory",
        "VK_KHR_external_semaphore",
        "VK_KHR_timeline_semaphore",
        "VK_KHR_dedicated_allocation",
        "VK_KHR_get_memory_requirements2",
        "VK_KHR_external_memory_fd",
        "VK_KHR_external_semaphore_fd",
        "VK_KHR_image_format_list"
    };

    uint32_t added = 0;
    for (int j = 0; j < 8; ++j) {
        int found = 0;
        for (uint32_t i = 0; i < ext_count; ++i) {
            if (strcmp(exts[i], xr_exts[j]) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            exts[ext_count + added] = xr_exts[j];
            added++;
        }
    }

    VkDeviceCreateInfo newCreateInfo = *pCreateInfo;
    newCreateInfo.enabledExtensionCount = ext_count + added;
    newCreateInfo.ppEnabledExtensionNames = exts;

    fprintf(stderr, "[vulkan_xr_hook] vkCreateDevice intercepted: added %u extensions (Total: %u)\n", added, ext_count + added);
    for (uint32_t i = 0; i < ext_count + added; ++i) {
        fprintf(stderr, "  %s\n", exts[i]);
    }

    VkResult res = real_vkCreateDevice(physicalDevice, &newCreateInfo, pAllocator, pDevice);
    free(exts);
    return res;
}

// Intercept vkGetInstanceProcAddr so that we return our wrapped versions of functions
PFN_vkVoidFunction vkGetInstanceProcAddr(VkInstance instance, const char* pName) {
    if (pName && strcmp(pName, "vkCreateInstance") == 0) {
        return (PFN_vkVoidFunction)vkCreateInstance;
    }
    if (pName && strcmp(pName, "vkCreateDevice") == 0) {
        return (PFN_vkVoidFunction)vkCreateDevice;
    }
    PFN_vkGetInstanceProcAddr real_vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(RTLD_NEXT, "vkGetInstanceProcAddr");
    if (real_vkGetInstanceProcAddr) {
        return real_vkGetInstanceProcAddr(instance, pName);
    }
    return NULL;
}
