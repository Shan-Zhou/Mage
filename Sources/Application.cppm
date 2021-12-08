module;
#define VK_USE_PLATFORM_WIN32_KHR
#include "vulkan/vulkan.h"
export module Application;
import Vulkan;
import Window;

#define DELETE_ALL_DEFAULT(T) \
	T() = delete; \
	T(const T&) = delete; \
	T& operator=(const T&) = delete


/*
* For unknown reason, the implicit conversion of vk::Instance (and other similar ones) prevents Intellisense to work property, so the app is tested in vulkan.ixx and then moved to this module
* One possible workaround is to carefully tune the API in Vulkan.ixx so that no C API is exported (this is a TODO)
*/
export namespace app {

}

module :private;
namespace app {

}