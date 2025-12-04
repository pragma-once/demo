#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "../sdl2/include/SDL.h"
#include "../sdl2/include/SDL_vulkan.h"

#include "event.h"
#include "window_settings.h"

namespace Engine
{
    /// @brief A thread-safe window class that contains the vulkan and sdl context.
    class Window final // TODO: Support vulkan validation layers, send problems through event?
    {
    public:
        class PhysicalDeviceInfo
        {
        public:
            VkSurfaceKHR surface;
            VkPhysicalDevice physical_device;
            VkPhysicalDeviceProperties properties;
            VkPhysicalDeviceFeatures features;
            std::vector<VkExtensionProperties> extensions;
            std::vector<VkQueueFamilyProperties> queue_families;
            VkSurfaceCapabilitiesKHR surface_capabilities;
            std::vector<VkSurfaceFormatKHR> surface_formats;
            std::vector<VkPresentModeKHR> surface_present_modes;
            PhysicalDeviceInfo(VkPhysicalDevice, VkSurfaceKHR);
        };

        Window(const std::string& title, const WindowSettings& settings);
        ~Window();

        Window(Window&) = delete;
        Window(Window&&) = delete;
        Window& operator=(Window&) = delete;
        Window& operator=(Window&&) = delete;

        /// @return whether the window is closed
        bool update(); // TODO
        void close(); // TODO
        bool is_closed();

        WindowSettings get_current_settings();
        std::vector<PhysicalDeviceInfo> get_supported_gpus();
        PhysicalDeviceInfo get_current_gpu();
        std::set<PresentMode> get_supported_present_modes();
        PresentMode get_current_present_mode();
    private:
        std::shared_mutex state_mutex;
        static int object_count;

        bool closed;

        std::string title;
        WindowSettings settings;
        SDL_Window * window;
        VkInstance instance;
        VkSurfaceKHR surface;
        std::vector<VkPhysicalDevice> physical_devices;
        std::map<VkPhysicalDevice, PhysicalDeviceInfo> supported_physical_devices;
        VkPhysicalDevice chosen_physical_device;
        VkDevice device;
        std::optional<std::uint32_t> graphics_queue_family_index;
        std::optional<std::uint32_t> surface_support_queue_family_index;
        VkQueue graphics_queue;
        VkQueue present_queue;
        VkSurfaceFormatKHR surface_format;
        VkPresentModeKHR surface_present_mode;
        VkExtent2D swap_chain_extent;
        VkSwapchainKHR swap_chain;
        std::vector<VkImageView> swap_chain_image_views;

        /// Called by destructor or in close event
        void destroy();
        /// Called by constructor
        void init_vulkan_instance();
        /// Requires init_vulkan_instance to be called before.
        /// Called by constructor
        void init_vulkan_surface();
        /// Requires init_vulkan_surface to be called before.
        /// Called by constructor
        void init_vulkan_device();
        /// Requires init_vulkan_device to be called before.
        /// Called by constructor or in resize event
        void init_vulkan_swap_chain();
        /// Called by destructor or in resize event
        void destroy_swap_chain();

        /// @brief Updates physical_devices and supported_physical_devices.
        void update_physical_devices();

        bool is_physical_device_supported(const PhysicalDeviceInfo&);
        /// @return A non-negative rank (0 or higher).
        int rank_physical_device(const PhysicalDeviceInfo&);
        /// @brief Chooses the best device based on rank_physical_device.
        VkPhysicalDevice choose_the_best_physical_device();
        /// @brief Chooses the device based on settings, if not found, using choose_the_best_physical_device.
        VkPhysicalDevice choose_physical_device();
        /// Requires init_vulkan_device to be called before.
        /// Called by init_vulkan_swap_chain
        VkSurfaceFormatKHR choose_surface_format();
    };
}
