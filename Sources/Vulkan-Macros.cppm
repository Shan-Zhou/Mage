/*
* Not used
*/
module;
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
export module Vulkan:Macros;

export namespace vk {
	constexpr auto KhrSurfaceSpecVersion		= VK_KHR_SURFACE_SPEC_VERSION;
	constexpr auto KhrSurfaceExtensionName		= VK_KHR_SURFACE_EXTENSION_NAME;
	constexpr auto KhrWin32SurfaceSpecVersion	= VK_KHR_WIN32_SURFACE_SPEC_VERSION;
	constexpr auto KhrWin32SurfaceExtensionName = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
}