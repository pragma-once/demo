#include "window.h"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace Engine
{
    int Window::object_count = 0;

    // -------- public members -------- //

    Window::PhysicalDeviceInfo::PhysicalDeviceInfo(VkPhysicalDevice physical_device, VkSurfaceKHR surface)
    {
        this->surface = surface;
        this->physical_device = physical_device;
        vkGetPhysicalDeviceProperties(physical_device, &properties);
        vkGetPhysicalDeviceFeatures(physical_device, &features);
        uint32_t extensions_count;
        vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extensions_count, nullptr);
        extensions.resize(extensions_count);
        vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extensions_count, extensions.data());

        std::uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
        queue_families.resize(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities);
        uint32_t format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);
        surface_formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, surface_formats.data());
        uint32_t present_mode_count;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr);
        surface_present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, surface_present_modes.data());
    }

    Window::Window(const std::string& title, const WindowSettings& settings)
        : window(NULL), instance(VK_NULL_HANDLE),
        surface(VK_NULL_HANDLE),
        chosen_physical_device(VK_NULL_HANDLE),
        device(VK_NULL_HANDLE),
        graphics_queue(VK_NULL_HANDLE),
        present_queue(VK_NULL_HANDLE),
        swap_chain(VK_NULL_HANDLE)
    {
        std::lock_guard<std::shared_mutex> lock(state_mutex);
        object_count++;
        int result = SDL_Init(SDL_INIT_VIDEO);
        if (result != 0)
            throw std::runtime_error(SDL_GetError());

        this->title = title;
        this->settings = settings;

        // Window
        window = SDL_CreateWindow(
            this->title.data(),
            this->settings.position_x,
            this->settings.position_y,
            this->settings.width,
            this->settings.height,
            SDL_WINDOW_VULKAN | this->settings.get_window_mode_flags()
        );
        if (window == NULL)
            throw std::runtime_error(SDL_GetError());
        // Determine the actual width and height.
        SDL_GetWindowSize(window, &this->settings.width, &this->settings.height);

        // Vulkan init
        init_vulkan_instance();
        init_vulkan_surface();
        init_vulkan_device();
        init_vulkan_swap_chain();
    }

    Window::~Window()
    {
        std::lock_guard<std::shared_mutex> lock(state_mutex);
        object_count--;
        destroy();
        if (object_count == 0)
            SDL_Quit();
    }

    bool Window::update()
    {
        std::lock_guard<std::shared_mutex> lock(state_mutex);
        SDL_Event e;
        if (window == NULL)
            return true;

        while (SDL_PollEvent(&e) != 0)
        {
            if (e.type == SDL_QUIT)
            {
                destroy();
                return true;
            }
        }

        return false;
    }

    void Window::close()
    {
        std::lock_guard<std::shared_mutex> lock(state_mutex);
        // TODO: handle stuff
        destroy();
    }

    bool Window::is_closed()
    {
        std::shared_lock<std::shared_mutex> lock(state_mutex);
        return window == NULL;
    }

    WindowSettings Window::get_current_settings()
    {
        std::shared_lock<std::shared_mutex> lock(state_mutex);
        return settings;
    }

    std::vector<Window::PhysicalDeviceInfo> Window::get_supported_gpus()
    {
        std::lock_guard<std::shared_mutex> lock(state_mutex);
        update_physical_devices();
        std::vector<Window::PhysicalDeviceInfo> result;
        for (auto item : supported_physical_devices)
            result.push_back(item.second);
        return result;
    }

    Window::PhysicalDeviceInfo Window::get_current_gpu()
    {
        std::shared_lock<std::shared_mutex> lock(state_mutex);
        return supported_physical_devices[chosen_physical_device];
    }

    std::set<PresentMode> Window::get_supported_present_modes()
    {
        std::shared_lock<std::shared_mutex> lock(state_mutex);
        return WindowSettings::convert_vk_present_modes_to_present_modes(
            supported_physical_devices[chosen_physical_device].surface_present_modes
        );
    }

    PresentMode Window::get_current_present_mode()
    {
        std::shared_lock<std::shared_mutex> lock(state_mutex);
        return WindowSettings::convert_vk_present_mode_to_present_mode(
            surface_present_mode
        );
    }

    // -------- private members -------- //

    void Window::destroy()
    {
        for (auto image_view : swap_chain_image_views)
            vkDestroyImageView(device, image_view, nullptr);
        destroy_swap_chain();
        if (device != VK_NULL_HANDLE)
            vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
        if (surface != VK_NULL_HANDLE)
            vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
        if (instance != VK_NULL_HANDLE)
            vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
        if (window != NULL)
            SDL_DestroyWindow(window);
        window = NULL;
    }

    void Window::init_vulkan_instance()
    {
        unsigned int extensions_count;
        if (!SDL_Vulkan_GetInstanceExtensions(window, &extensions_count, nullptr))
            throw std::runtime_error("Could not get instance extensions count.");
        std::vector<const char*> extensions(extensions_count);
        if (!SDL_Vulkan_GetInstanceExtensions(window, &extensions_count, extensions.data()))
            throw std::runtime_error("Could not get instance extensions.");

        VkApplicationInfo app_info {};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Test";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "No Engine";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_3;
        VkInstanceCreateInfo create_info {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &app_info;
        create_info.enabledExtensionCount = extensions.size();
        create_info.ppEnabledExtensionNames = extensions.data();
        create_info.enabledLayerCount = 0; // Can be used for validation layers, update the device's create_info of this is changed?
        VkResult result = vkCreateInstance(&create_info, nullptr, &instance);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Could not create vk instance.");
        }
    }

    void Window::init_vulkan_surface()
    {
        if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) // SDL func here
        {
            throw std::runtime_error("Could not create window surface.");
        }
    }

    const std::vector<const char*> REQUIRED_DEVICE_EXTENSIONS = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    void Window::init_vulkan_device()
    {
        update_physical_devices();
        chosen_physical_device = choose_physical_device();
        settings.gpu_vendor_id = supported_physical_devices[chosen_physical_device].properties.vendorID;
        settings.gpu_device_id = supported_physical_devices[chosen_physical_device].properties.deviceID;
        settings.gpu_name = supported_physical_devices[chosen_physical_device].properties.deviceName;

        graphics_queue_family_index.reset();
        surface_support_queue_family_index.reset();
        for (int i = 0; i < supported_physical_devices[chosen_physical_device].queue_families.size(); i++)
        {
            if (supported_physical_devices[chosen_physical_device].queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                graphics_queue_family_index = i;
            }
            VkBool32 surface_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(chosen_physical_device, i, surface, &surface_support);
            if (surface_support)
            {
                surface_support_queue_family_index = i;
            }

            if (graphics_queue_family_index.has_value() && surface_support_queue_family_index.has_value())
                break;
        }
        if (!graphics_queue_family_index.has_value() || !surface_support_queue_family_index.has_value())
            throw std::runtime_error("Could not find graphics and/or surface support queues.");

        std::vector<VkDeviceQueueCreateInfo> device_queue_create_infos;
        std::set<uint32_t> unique_queue_family_indices = { // NOTE: It's a set (unique indices)
            graphics_queue_family_index.value(),
            surface_support_queue_family_index.value()
        };
        float queue_priorities[] = { 1 }; // in range [0, 1]
        for (uint32_t queue_family_index : unique_queue_family_indices)
        {
            VkDeviceQueueCreateInfo device_queue_create_info {};
            device_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            device_queue_create_info.queueFamilyIndex = queue_family_index;
            device_queue_create_info.queueCount = 1;
            device_queue_create_info.pQueuePriorities = queue_priorities;
            device_queue_create_infos.push_back(device_queue_create_info);
        }
        VkPhysicalDeviceFeatures device_features {};
        VkDeviceCreateInfo device_create_info {};
        device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_create_info.queueCreateInfoCount = device_queue_create_infos.size();
        device_create_info.pQueueCreateInfos = device_queue_create_infos.data();
        device_create_info.pEnabledFeatures = &device_features;
        device_create_info.enabledExtensionCount = REQUIRED_DEVICE_EXTENSIONS.size();
        device_create_info.ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS.data();
        device_create_info.enabledLayerCount = 0; // setting to the same as instance create info for compatibility with old vulkan
        if (vkCreateDevice(chosen_physical_device, &device_create_info, nullptr, &device) != VK_SUCCESS)
        {
            throw std::runtime_error("Could not create logical device.");
        }
        // graphics_queue/present_queue: automatically destroyed with device
        vkGetDeviceQueue(device, graphics_queue_family_index.value(), 0, &graphics_queue);
        vkGetDeviceQueue(device, surface_support_queue_family_index.value(), 0, &present_queue);
    }

    void Window::destroy_swap_chain()
    {
        if (swap_chain != VK_NULL_HANDLE)
            vkDestroySwapchainKHR(device, swap_chain, nullptr);
        swap_chain = VK_NULL_HANDLE;
    }

    void Window::init_vulkan_swap_chain()
    {
        surface_format = choose_surface_format();
        surface_present_mode = VK_PRESENT_MODE_FIFO_KHR;
        VkPresentModeKHR target_present_mode = settings.get_vk_present_mode();
        for (const auto& item : supported_physical_devices[chosen_physical_device].surface_present_modes)
        {
            if (target_present_mode == item)
                surface_present_mode = target_present_mode;
        }
        // Set swap_chain_extent (resolution)
        const auto& capabilities = supported_physical_devices[chosen_physical_device].surface_capabilities;
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()
            && capabilities.currentExtent.height != std::numeric_limits<uint32_t>::max())
        {
            // Vulkan tells to match the resolution of the window through currentExtent.
            swap_chain_extent = capabilities.currentExtent;
        }
        else
        {
            int width, height;
            SDL_Vulkan_GetDrawableSize(window, &width, &height);
            VkExtent2D result = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };
            result.width = std::clamp(result.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            result.height = std::clamp(result.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
            swap_chain_extent = result;
        }

        // Create swap chain

        uint32_t image_count = capabilities.minImageCount;
        if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount)
            image_count = capabilities.maxImageCount;
        VkSwapchainCreateInfoKHR swap_chain_create_info {};
        swap_chain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swap_chain_create_info.surface = surface;
        swap_chain_create_info.minImageCount = image_count;

        swap_chain_create_info.imageFormat = surface_format.format;
        swap_chain_create_info.imageColorSpace = surface_format.colorSpace;
        swap_chain_create_info.imageExtent = swap_chain_extent;
        // layers for each image, 1, unless stereoscopic 3D
        swap_chain_create_info.imageArrayLayers = 1;
        // can be VK_IMAGE_USAGE_TRANSFER_DST_BIT to be transfered:
        swap_chain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        uint32_t queue_family_indexes[] = {
            graphics_queue_family_index.value(), surface_support_queue_family_index.value()
        };
        if (graphics_queue_family_index.value() != surface_support_queue_family_index.value())
        {
            swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swap_chain_create_info.queueFamilyIndexCount = 2;
            swap_chain_create_info.pQueueFamilyIndices = queue_family_indexes;
            // TODO
            // Using the concurrent mode to avoid having to manage ownership.
        }
        else
        {
            swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // This option offers the best performance.
            swap_chain_create_info.queueFamilyIndexCount = 0;
            swap_chain_create_info.pQueueFamilyIndices = nullptr;
        }

        // No transform done on image (else: capabilities.supportedTransforms)
        swap_chain_create_info.preTransform = capabilities.currentTransform;
        // Specifies if the alpha channel should be used for blending with other windows in the window system.
        // VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR => ignore the alpha channel
        swap_chain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        swap_chain_create_info.presentMode = surface_present_mode;
        // VK_TRUE: We don't care about the color of pixels that are obscured,
        // for example because another window is in front of them.
        // Unless you really need to be able to read these pixels back and get predictable results,
        // you'll get the best performance by enabling clipping.
        swap_chain_create_info.clipped = VK_TRUE;

        // With Vulkan it's possible that your swap chain becomes invalid or unoptimized while your application is running,
        // for example because the window was resized. In that case the swap chain actually needs to be recreated from scratch
        // and a reference to the old one must be specified in this field.
        // Learn more: https://vulkan-tutorial.com/en/Drawing_a_triangle/Swap_chain_recreation
        swap_chain_create_info.oldSwapchain = swap_chain; // VK_NULL_HANDLE if destroyed / not initialized

        if (vkCreateSwapchainKHR(device, &swap_chain_create_info, nullptr, &swap_chain) != VK_SUCCESS)
        {
            throw std::runtime_error("Could not create swap chain.");
        }

        // will be automatically cleaned up once the swap chain has been destroyed
        std::vector<VkImage> swap_chain_images;

        vkGetSwapchainImagesKHR(device, swap_chain, &image_count, nullptr);
        swap_chain_images.resize(image_count);
        vkGetSwapchainImagesKHR(device, swap_chain, &image_count, swap_chain_images.data());

        swap_chain_image_views.resize(swap_chain_images.size());
        for (int i = 0; i < swap_chain_images.size(); i++)
        {
            VkImageViewCreateInfo create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            create_info.image = swap_chain_images[i];
            create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            create_info.format = surface_format.format;
            // Can lock components to 0/1, connect to other component, etc.
            create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            // Color? depth? ...?
            create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            // No mipmapping / layers
            // Stereographic 3D would have multiple layers,
            // then multiple image views for each
            create_info.subresourceRange.baseMipLevel = 0;
            create_info.subresourceRange.levelCount = 1;
            create_info.subresourceRange.baseArrayLayer = 0;
            create_info.subresourceRange.layerCount = 1;
            if (vkCreateImageView(device, &create_info, nullptr, &swap_chain_image_views[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("Could not create image views.");
            }
        }
    }

    void Window::update_physical_devices()
    {
        std::uint32_t devices_count;
        vkEnumeratePhysicalDevices(instance, &devices_count, nullptr);
        if (devices_count == 0)
        {
            throw std::runtime_error("Could not find any GPU with Vulkan support.");
        }
        physical_devices.resize(devices_count);
        vkEnumeratePhysicalDevices(instance, &devices_count, physical_devices.data());
        supported_physical_devices.clear();
        for (auto device : physical_devices)
        {
            auto device_info = PhysicalDeviceInfo(device, surface);
            if (is_physical_device_supported(device_info))
            {
                supported_physical_devices[device] = device_info;
            }
        }
        if (supported_physical_devices.empty())
        {
            throw std::runtime_error("Could not find any supported GPU.");
        }
    }

    inline bool check_required_device_extensions(const std::vector<VkExtensionProperties>& device_extensions)
    {
        std::set<std::string> required_extensions_set(REQUIRED_DEVICE_EXTENSIONS.begin(), REQUIRED_DEVICE_EXTENSIONS.end());
        for (const auto& extension : device_extensions)
            required_extensions_set.erase(extension.extensionName);
        return required_extensions_set.empty();
    }
    inline bool check_device_queue_families(
            const std::vector<VkQueueFamilyProperties>& queue_families,
            const VkPhysicalDevice physical_device,
            const VkSurfaceKHR surface
        )
    {
        bool graphics_queue_family_exists = false;
        bool surface_support_queue_family_exists = false;
        for (int i = 0; i < queue_families.size(); i++)
        {
            if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                graphics_queue_family_exists = true;
            }
            VkBool32 surface_support = true;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &surface_support);
            if (surface_support)
            {
                surface_support_queue_family_exists = true;
            }

            if (graphics_queue_family_exists && surface_support_queue_family_exists)
                return true;
        }
        return false;
    }
    inline bool check_device_surface_support(std::vector<VkSurfaceFormatKHR> surface_formats, std::vector<VkPresentModeKHR> surface_present_modes)
    {
        return !surface_formats.empty() && !surface_present_modes.empty();
    }
    bool Window::is_physical_device_supported(const PhysicalDeviceInfo& device_info)
    {
        return device_info.features.geometryShader
                && check_required_device_extensions(device_info.extensions)
                && check_device_queue_families(device_info.queue_families, device_info.physical_device, device_info.surface)
                && check_device_surface_support(device_info.surface_formats, device_info.surface_present_modes);
    }

    int Window::rank_physical_device(const PhysicalDeviceInfo& device_info)
    {
        int rank = 0;
        if (device_info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            rank += 1000;
        rank += device_info.properties.limits.maxImageDimension2D;
        return rank;
    }

    VkPhysicalDevice Window::choose_the_best_physical_device()
    {
        int max_rank = -1;
        VkPhysicalDevice result = VK_NULL_HANDLE;
        for (auto physical_device : supported_physical_devices)
        {
            int rank = rank_physical_device(physical_device.second);
            if (rank > max_rank)
            {
                max_rank = rank;
                result = physical_device.first;
            }
        }
        return result;
    }

    VkPhysicalDevice Window::choose_physical_device()
    {
        VkPhysicalDevice chosen_device = VK_NULL_HANDLE;
        // Choosing attempt 1: ID
        for (auto physical_device : supported_physical_devices)
        {
            if (physical_device.second.properties.vendorID == settings.gpu_vendor_id
                && physical_device.second.properties.deviceID == settings.gpu_device_id)
            {
                chosen_device = physical_device.first;
                break;
            }
        }
        if (chosen_device == VK_NULL_HANDLE)
        {
            // Choosing attempt 2: Name
            for (auto physical_device : supported_physical_devices)
            {
                if (std::string(physical_device.second.properties.deviceName) == settings.gpu_name)
                {
                    chosen_device = physical_device.first;
                    break;
                }
            }
            if (chosen_device == VK_NULL_HANDLE)
            {
                // Choosing attempt 3: Rank and choose
                chosen_device = choose_the_best_physical_device();
                if (chosen_device == VK_NULL_HANDLE)
                {
                    throw std::runtime_error("Could not choose a GPU. This is probably a bug.");
                }
            }
        }
        return chosen_device;
    }

    VkSurfaceFormatKHR Window::choose_surface_format()
    {
        const auto& formats = supported_physical_devices[chosen_physical_device].surface_formats;
        for (const auto& format : formats)
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                return format;
        return formats[0];
    }
}
