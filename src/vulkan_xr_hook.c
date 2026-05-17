/* vulkan_xr_hook.c
 * Intercepts Vulkan instance/device creation to inject required OpenXR extensions.
 * On Android this file is compiled directly into libmain.so. Since libvulkan.so is
 * loaded by the platform and Dawn calls vkCreateInstance/vkCreateDevice by symbol
 * name within the same SO, our exported symbols shadow the libvulkan.so ones and
 * these hooks are called first. We then forward to the real functions obtained via
 * dlopen("libvulkan.so", RTLD_NOLOAD).
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <vulkan/vulkan.h>
#ifdef __ANDROID__
#include <android/log.h>
#define LOGHOOK(...) __android_log_print(ANDROID_LOG_INFO,  "vulkan_xr_hook", __VA_ARGS__)
#else
#define LOGHOOK(...) fprintf(stderr, "[vulkan_xr_hook] " __VA_ARGS__), fputc('\n', stderr)
#endif

/* ------------------------------------------------------------------ */
/* Helpers to get the real Vulkan function pointers from libvulkan.so  */
/* ------------------------------------------------------------------ */

static void* get_libvulkan(void) {
    static void* lib = NULL;
    if (!lib) {
        /* RTLD_NOLOAD returns the already-opened libvulkan without a new dlopen.
         * On Android libvulkan.so is always pre-loaded by the platform. */
        lib = dlopen("libvulkan.so", RTLD_NOW | RTLD_NOLOAD);
        if (!lib) lib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_NOLOAD);
        /* Last resort: open it fresh */
        if (!lib) lib = dlopen("libvulkan.so", RTLD_NOW);
    }
    return lib;
}

static void* vulkan_sym(const char* name) {
    void* lib = get_libvulkan();
    if (!lib) return NULL;
    return dlsym(lib, name);
}

/* ------------------------------------------------------------------ */
/* Forward declarations                                                */
/* ------------------------------------------------------------------ */

VkResult vkCreateInstance(const VkInstanceCreateInfo*, const VkAllocationCallbacks*, VkInstance*);
VkResult vkCreateDevice(VkPhysicalDevice, const VkDeviceCreateInfo*, const VkAllocationCallbacks*, VkDevice*);
PFN_vkVoidFunction vkGetInstanceProcAddr(VkInstance, const char*);

/* ------------------------------------------------------------------ */
/* vkCreateInstance interceptor                                        */
/* ------------------------------------------------------------------ */

VkResult vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo,
                           const VkAllocationCallbacks* pAllocator,
                           VkInstance* pInstance) {
    PFN_vkCreateInstance real = (PFN_vkCreateInstance)vulkan_sym("vkCreateInstance");
    if (!real) {
        LOGHOOK("ERROR: real vkCreateInstance not found");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t ext_count = pCreateInfo->enabledExtensionCount;
    const char** exts = (const char**)malloc(sizeof(char*) * (ext_count + 16));
    for (uint32_t i = 0; i < ext_count; ++i)
        exts[i] = pCreateInfo->ppEnabledExtensionNames[i];

    const char* want[] = {
        "VK_KHR_external_memory_capabilities",
        "VK_KHR_get_physical_device_properties2",
        "VK_KHR_external_fence_capabilities",
        "VK_KHR_external_semaphore_capabilities",
    };
    const int want_count = 4;

    uint32_t added = 0;
    for (int j = 0; j < want_count; ++j) {
        int found = 0;
        for (uint32_t i = 0; i < ext_count; ++i)
            if (strcmp(exts[i], want[j]) == 0) { found = 1; break; }
        if (!found)
            exts[ext_count + added++] = want[j];
    }

    VkInstanceCreateInfo ci = *pCreateInfo;
    ci.enabledExtensionCount = ext_count + added;
    ci.ppEnabledExtensionNames = exts;

    VkApplicationInfo ai;
    if (pCreateInfo->pApplicationInfo) {
        ai = *pCreateInfo->pApplicationInfo;
        if (ai.apiVersion < VK_API_VERSION_1_1)
            ai.apiVersion = VK_API_VERSION_1_1;
        ci.pApplicationInfo = &ai;
    }

    LOGHOOK("vkCreateInstance: injected %u instance exts (total %u)", added, ext_count + added);

    VkResult res = real(&ci, pAllocator, pInstance);
    free(exts);
    return res;
}

/* ------------------------------------------------------------------ */
/* vkCreateDevice interceptor                                          */
/* ------------------------------------------------------------------ */

VkResult vkCreateDevice(VkPhysicalDevice physicalDevice,
                         const VkDeviceCreateInfo* pCreateInfo,
                         const VkAllocationCallbacks* pAllocator,
                         VkDevice* pDevice) {
    PFN_vkCreateDevice real = (PFN_vkCreateDevice)vulkan_sym("vkCreateDevice");
    if (!real) {
        LOGHOOK("ERROR: real vkCreateDevice not found");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t ext_count = pCreateInfo->enabledExtensionCount;
    const char** exts = (const char**)malloc(sizeof(char*) * (ext_count + 16));
    for (uint32_t i = 0; i < ext_count; ++i)
        exts[i] = pCreateInfo->ppEnabledExtensionNames[i];

    const char* want[] = {
        "VK_KHR_external_memory",
        "VK_KHR_external_semaphore",
        "VK_KHR_timeline_semaphore",
        "VK_KHR_dedicated_allocation",
        "VK_KHR_get_memory_requirements2",
        "VK_KHR_image_format_list",
#ifdef __ANDROID__
        "VK_ANDROID_external_memory_android_hardware_buffer",
#else
        "VK_KHR_external_memory_fd",
        "VK_KHR_external_semaphore_fd",
#endif
    };
#ifdef __ANDROID__
    const int want_count = 7;
#else
    const int want_count = 8;
#endif

    uint32_t added = 0;
    for (int j = 0; j < want_count; ++j) {
        int found = 0;
        for (uint32_t i = 0; i < ext_count; ++i)
            if (strcmp(exts[i], want[j]) == 0) { found = 1; break; }
        if (!found)
            exts[ext_count + added++] = want[j];
    }

    VkDeviceCreateInfo ci = *pCreateInfo;
    ci.enabledExtensionCount = ext_count + added;
    ci.ppEnabledExtensionNames = exts;

    LOGHOOK("vkCreateDevice: injected %u device exts (total %u)", added, ext_count + added);

    VkResult res = real(physicalDevice, &ci, pAllocator, pDevice);
    free(exts);
    return res;
}

/* ------------------------------------------------------------------ */
/* vkGetInstanceProcAddr interceptor                                   */
/* ------------------------------------------------------------------ */

PFN_vkVoidFunction vkGetInstanceProcAddr(VkInstance instance, const char* pName) {
    if (pName) {
        if (strcmp(pName, "vkCreateInstance") == 0)
            return (PFN_vkVoidFunction)vkCreateInstance;
        if (strcmp(pName, "vkCreateDevice") == 0)
            return (PFN_vkVoidFunction)vkCreateDevice;
    }
    PFN_vkGetInstanceProcAddr real =
        (PFN_vkGetInstanceProcAddr)vulkan_sym("vkGetInstanceProcAddr");
    if (real)
        return real(instance, pName);
    return NULL;
}
