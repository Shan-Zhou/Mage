// 
// This is a C++20 Vulkan + ImGui(not finished yet) wrapper.
// 
// Since some C++20 features have not been supported by all the compilers, some modifications might be necessary if you try to compile with gcc or clang.
// 
// Currently only tested with Visual Studio 2022. Visual Studio 2019 should work too (at least without much pain).
// 
// TODO: only export C++ API
// 

//
// 
//
// Contents:
// 
// ---- #Global_module_fragment
// ---- #Standard_library_workarounds
// ---- #Type_labels
// ---- #Colorful_log
//
// ---- #Vulkan_wrapper
// ---- #Raytracing
// ---- #Application
// ---- ---- #Scene
// ---- ---- #Entry_point_and_app_info
//
// ---- #Details
//


//  
//  #Global_module_fragment :
// 
//  {fmt} 8.0.0 could be compiled as a C++20 module but it seems to be still experimental, so it's included as usual libraries and in CMakeLists.txt we explicitly (set FMT_CAN_MODULE OFF).
// 
//  
// 
module;
#if defined(_WIN32)
#   define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__linux__) || defined(__unix__)
#   define VK_USE_PLATFORM_XLIB_KHR
#elif defined(__APPLE__)
#   define VK_USE_PLATFORM_MACOS_MVK
#else
#   error "Platform not supported by this example."
#endif

#define VOLK_IMPLEMENTATION
#include <volk.h>
//#include <vulkan/vulkan.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <fmt/format.h>
#include <fmt/color.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
//#define IMGUI_UNLIMITED_FRAME_RATE
#ifdef _DEBUG
#define IMGUI_VULKAN_DEBUG_REPORT
#endif
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#define STB_IMAGE_IMPLEMENTATION
#include "tinyobj_loader_opt.hpp"
#include <stb_image.h>

// GLSL
#include "glsl_cpp_common.h"

// I could have "import std.core;" but it would destroy Intellisense...
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

//#include <compare>
export module Vulkan;
import :Error;
//export import :Macros;
import Window;
// TODO: import std.core;

// standard library workarounds
// TODO: support clang (when it provides complete support for C++20 Modules)

#if defined( _MSVC_LANG )
#   define VULKAN_MODULE_CPLUSPLUS _MSVC_LANG
#   define RESERVED_LITERAL_WARNING 4455
#else
#   define VULKAN_MODULE_CPLUSPLUS __cplusplus
#endif

#if VULKAN_MODULE_CPLUSPLUS > 202300L
#   define VULKAN_MODULE_CPP_VERSION 23
#elif VULKAN_MODULE_CPLUSPLUS > 202000L
#   define VULKAN_MODULE_CPP_VERSION 20
#else
#   error Vulkan Module needs at least C++20
#endif


// for compilers that haven't implemented <ranges> yet, <ranges-v3> can be used instead
#if (VULKAN_MODULE_CPP_VERSION >= 20) && __has_include(<ranges>)
using std::views::iota;
using std::views::reverse;
#elif defined(RANGES_V3_VIEW_IOTA_HPP)
using views::iota;
using views::reverse;
#endif


// std::size_t unsigned literal
#if (VULKAN_MODULE_CPP_VERSION >= 23) && __has_include(<type_traits>)
    // use standard library <type_traits>
#else
#pragma warning (push)
#pragma warning (disable: RESERVED_LITERAL_WARNING) // Non-ANSI characters
constexpr std::size_t operator ""uz (unsigned long long n) {
    return n;
}
constexpr std::size_t operator ""zu (unsigned long long n) {
    return n;
}
#pragma warning (pop)
#endif

//
// #Type_labels
// 
namespace vk {
    // TODO: remove reference for plain old types
    template<typename T>
    struct type_converter {
        using input_type  = std::remove_reference_t<std::remove_const_t<T>> const&;
        using output_type = std::remove_reference_t<std::remove_const_t<T>>&;
        using inout_type  = std::remove_reference_t<std::remove_const_t<T>>&;
    };
    template<typename T>
    using in_t = type_converter<T>::input_type;
    template<typename T>
    using out_t = type_converter<T>::output_type;
    template<typename T>
    using inout_t = type_converter<T>::inout_type;
}
#undef IN 
#undef OUT
#undef INOUT
#define IN  /*input variable*/
#define OUT /*output variable*/
#define INOUT

#define VK_ENABLE_ERROR_CHECK
namespace vk {
#ifdef _DEBUG
	constexpr bool debugMode = true;
#else
	constexpr bool debugMode = false;
#endif
}

// 
// #Colorful_log
// 
export namespace vk {

	template <typename S, typename... Args>
	void LogInfo(const S& format_str, const Args&... args) {
		fmt::print(stdout, fmt::fg(fmt::color::sky_blue), format_str, args...);
	}

	template <typename S, typename... Args>
	void LogColorfulInfo(fmt::color color, const S& format_str, const Args&... args) {
		fmt::print(stdout, fmt::fg(color), format_str, args...);
	}

	template <typename S, typename... Args>
	void LogWarning(const S& format_str, const Args&... args) {
		fmt::print(stdout, fmt::fg(fmt::color::orange), format_str, args...);
	}

	template <typename S, typename... Args>
	void LogError(const S& format_str, const Args&... args) {
		fmt::print(stdout, fmt::fg(fmt::color::red), format_str, args...);
	}

	template <typename S, typename... Args>
	void LogLE(bool enabled, const S& format_str, const Args&... args) {
		fmt::print(stdout, fmt::fg(enabled ? fmt::color::light_green : fmt::color::sky_blue), format_str, args...);
	}

} // export namespace vk

namespace vk {
	// 
	// operator| is overloaded and should be viewed as a pipe operator
	// Now VkResult is checked by "(expression returning VkResult) | VK_NO_ERROR;"
	// An exception will be thrown if the checked expression returns VK_ERROR_...
	// The syntax is similar to .expect() in Rust
	// In non-debug mode, an almost null overload will be invoked instead
	// The pipe operator simply pass the VkResult and the compiler is expected to
	// optimize out the error-checking
	//

#ifdef VK_ENABLE_ERROR_CHECK
#	define VK_NO_ERROR (std::make_pair(__FILE__, __LINE__))
#else
#	define VK_NO_ERROR nullptr
#endif


	template<typename T>
	concept Pair = requires (T a) { a.first, a.second; };

	template<Pair T>
	VkResult operator|(VkResult result, const T& location) {
		return checkResult(result, location.first, location.second);
	}

	VkResult operator|(VkResult result, std::nullptr_t null) {
		return result;
	}

	// 
	// result | LOG_UNSUCCESS will LogWarning(result_name) if result != VK_SUCCESS
	// 
	//constexpr auto LOG_UNSUCCESS = [](const char* m) { LogWarning(m); };
	//VkResult operator|(VkResult result, const std::function<void(const char*)>& log) {
	//	logResult(result, log);
	//	return result;
	//}
//#	define CHECK_RESULT (std::make_pair(__FILE__, __LINE__))
} // namespace vk


// 
// #Vulkan_wrapper
//

//
// Delete all special member functions
// for classes that only have one or a few instances
// e.g. Instance, Device, Surface, Swapchain...
//
#define VK_DELETE_ALL_DEFAULT(T)        \
	T() = delete;                    \
	T(const T&) = delete;            \
	T& operator=(const T&) = delete; \
    T(const T&&) = delete;           \
	T& operator=(const T&&) = delete \

#if !__has_include(<volk.h>)
#   define VK_DECLARE(FunctionName)                              PFN_##FunctionName m_##FunctionName{nullptr}
#   define VK_LOAD_INSTANCE_EXT_FUNCTION(instance, FunctionName) m_##FunctionName = (PFN_##FunctionName)vkGetInstanceProcAddr(instance,#FunctionName)
#   define VK_LOAD_DEVICE_EXT_FUNCTION(device, FunctionName)     m_##FunctionName = (PFN_##FunctionName)vkGetDeviceProcAddr(device,#FunctionName)
#endif

export namespace vk {

    void InitVolkAndCheckVulkanVersion(uint32_t minimum_major, uint32_t minimum_minor);

	struct OptionalName {
		const char* name{ nullptr };
		bool optional{ false };
	};
	// It seems that Intellisense doesn't work very well with type alias of std::vector<T>
	// But I cannot reproduce the bug determinatedly, when the code size is small it doesn't appear
	//using OptionalNameList = std::vector<OptionalName>;
	//#define OptionalNameList std::vector<OptionalName>
	using NameList = std::vector<const char*>;
	//#define NameList std::vector<const char*>
	using GpuList = std::vector<VkPhysicalDevice>;
	//#define GpuList std::vector<VkPhysicalDevice>

    using QueueFamilyIndexType = uint32_t;

#define VK_TYPE_ALIAS_DEFINE(Typename) using Typename = Vk##Typename;
#define VMA_TYPE_ALIAS_DEFINE(Typename) using Typename = Vma##Typename;

    VK_TYPE_ALIAS_DEFINE(Extent2D);
    VK_TYPE_ALIAS_DEFINE(Extent3D);
    auto Extent2Dto3D(Extent2D extent) -> Extent3D {
        return Extent3D{
            .width  = extent.width,
            .height = extent.height,
            .depth  = 1,
        };
    }

    VK_TYPE_ALIAS_DEFINE(DeviceAddress);

    VK_TYPE_ALIAS_DEFINE(DeviceSize);
    constexpr DeviceSize no_offset = 0;

    VK_TYPE_ALIAS_DEFINE(SampleMask);

    VK_TYPE_ALIAS_DEFINE(MemoryMapFlags);
    constexpr MemoryMapFlags no_memory_map_flags = 0;

    VK_TYPE_ALIAS_DEFINE(Flags);
    constexpr Flags no_flags = 0;
    // TODO: Move to Vulkan-Enums.cppm;
#pragma region Enums
#define VK_FLAG_BITS_OPERATORS_DEFINE(EnumName)                                        \
    constexpr EnumName##Flags operator+ (EnumName##FlagBits a) {                       \
        return static_cast<EnumName##Flags>(a);                                        \
    }                                                                                  \
    constexpr EnumName##Flags operator| (EnumName##FlagBits a, EnumName##FlagBits b) { \
        return static_cast<EnumName##Flags>(a) | static_cast<EnumName##Flags>(b);      \
    }                                                                                  \
    constexpr EnumName##Flags operator| (EnumName##Flags a, EnumName##FlagBits b) {    \
        return a | static_cast<EnumName##Flags>(b);                                    \
    }                                                                                  \
    constexpr EnumName##Flags operator| (EnumName##FlagBits a, EnumName##Flags b) {    \
        return static_cast<EnumName##Flags>(a) | b;                                    \
    }                                                                                  \
    EnumName##Flags& operator|= (EnumName##Flags& a, EnumName##FlagBits b) { \
        return a = a | b;                                                    \
    }                                                                        \

#define VMA_SCOPED_ENUM_OPERATORS_DEFINE(EnumName)   \
    constexpr Vma##EnumName operator+ (EnumName e) { \
        return static_cast<Vma##EnumName>(e);        \
    }                                                \

    VK_TYPE_ALIAS_DEFINE(FenceCreateFlags);
    enum class FenceCreateFlagBits : FenceCreateFlags {
        Unsignaled = 0x00000000,
        Signaled   = 0x00000001,
        MaxEnum    = 0x7FFFFFFF,
    };
    VK_FLAG_BITS_OPERATORS_DEFINE(FenceCreate);

    VK_TYPE_ALIAS_DEFINE(CommandBufferUsageFlags);
    enum class CommandBufferUsageFlagBits : CommandBufferUsageFlags {
        OneTimeSubmit      = 0x00000001,
        RenderPassContinue = 0x00000002,
        SimultaneousUse    = 0x00000004,
        MaxEnum            = 0x7FFFFFFF
    };
    VK_FLAG_BITS_OPERATORS_DEFINE(CommandBufferUsage);

    VMA_TYPE_ALIAS_DEFINE(AllocatorCreateFlags);
    enum class AllocatorCreateFlagBits : AllocatorCreateFlags {
        ExternallySynchronized  = 0x00000001,
        KhrDedicatedAllocation  = 0x00000002,
        KhrBindMemory2          = 0x00000004,
        ExtMemoryBudget         = 0x00000008,
        AmdDeviceCoherentMemory = 0x00000010,
        BufferDeviceAddress     = 0x00000020,
        ExtMemoryPriority       = 0x00000040,
        MaxEnum                 = 0x7FFFFFFF
    };
    VK_FLAG_BITS_OPERATORS_DEFINE(AllocatorCreate);

    enum class MemoryUsage {
        Unknown            = 0,
        GpuOnly            = 1,
        CpuOnly            = 2,
        CpuToGpu           = 3,
        GpuToCpu           = 4,
        CpuCopy            = 5,
        GpuLazilyAllocated = 6,
        MaxEnum            = 0x7FFFFFFF
    };
    VMA_SCOPED_ENUM_OPERATORS_DEFINE(MemoryUsage);
    //constexpr VmaMemoryUsage operator+(MemoryUsage usage) {
    //    return static_cast<VmaMemoryUsage>(usage);
    //}

    VK_TYPE_ALIAS_DEFINE(MemoryPropertyFlags);
    enum class MemoryPropertyFlagBits : MemoryPropertyFlags {
        DeviceLocal       = 0x00000001,
        HostVisible       = 0x00000002,
        HostCoherent      = 0x00000004,
        HostCached        = 0x00000008,
        LazilyAllocated   = 0x00000010,
        Protected         = 0x00000020,
        DeviceCoherentAmd = 0x00000040,
        DeviceUncachedAmd = 0x00000080,
        RdmaCapableNv     = 0x00000100,
        FlagBitsMax       = 0x7FFFFFFF
    };
    VK_FLAG_BITS_OPERATORS_DEFINE(MemoryProperty);

    VK_TYPE_ALIAS_DEFINE(BufferUsageFlags);
	enum class BufferUsageFlagBits : BufferUsageFlags {
		TransferSrc                                = 0x00000001,
		TransferDst                                = 0x00000002,
		UniformTexelBuffer                         = 0x00000004,
		StorageTexelBuffer                         = 0x00000008,
		UniformBuffer                              = 0x00000010,
		StorageBuffer                              = 0x00000020,
		IndexBuffer                                = 0x00000040,
		VertexBuffer                               = 0x00000080,
		IndirectBuffer                             = 0x00000100,
		ShaderDeviceAddress                        = 0x00020000,
		VideoDecodeSrcKhr                          = 0x00002000,
		VideoDecodeDstKhr                          = 0x00004000,
		TransformFeedbackBufferExt                 = 0x00000800,
		TransformFeedbackCounterBufferExt          = 0x00001000,
		ConditionalRenderingExt                    = 0x00000200,
		AccelerationStructureBuildInputReadOnlyKhr = 0x00080000,
		AccelerationStructureStorageKhr            = 0x00100000,
		ShaderBindingTableKhr                      = 0x00000400,
		VideoEncodeDstKhr                          = 0x00008000,
		VideoEncodeSrcKhr                          = 0x00010000,
		RayTracingNv                               = ShaderBindingTableKhr,
		ShaderDeviceAddressExt                     = ShaderDeviceAddress,
		ShaderDeviceAddressKhr                     = ShaderDeviceAddress,
		MaxEnum                                    = 0x7FFFFFFF
	};
    VK_FLAG_BITS_OPERATORS_DEFINE(BufferUsage);

    VK_TYPE_ALIAS_DEFINE(ImageUsageFlags);
    enum class ImageUsageFlagBits : ImageUsageFlags {
        TransferSrc = 0x00000001,
        TransferUst = 0x00000002,
        Sampled = 0x00000004,
        Storage = 0x00000008,
        ColorAttachment = 0x00000010,
        DepthStencilAttachment = 0x00000020,
        TransientAttachment = 0x00000040,
        InputAttachment = 0x00000080,
        VideoDecodeDstKhr = 0x00000400,
        VideoDecodeSrcKhr = 0x00000800,
        VideoDecodeDpbKhr = 0x00001000,
        FragmentDensityMapExt = 0x00000200,
        FragmentShadingRateAttachmentKhr = 0x00000100,
        VideoEncodeDstKhr = 0x00002000,
        VideoEncodeSrcKhr = 0x00004000,
        VideoEncodeDpbKhr = 0x00008000,
        InvocationMaskHuawei = 0x00040000,
        ShadingRateImageNv = FragmentShadingRateAttachmentKhr,
        MaxEnum = 0x7FFFFFFF
    };
    VK_FLAG_BITS_OPERATORS_DEFINE(ImageUsage);


    VK_TYPE_ALIAS_DEFINE(QueueFlags);
	enum class QueueFlagBits : QueueFlags {
		Graphics       = 0x00000001,
		Compute        = 0x00000002,
		Transfer       = 0x00000004,
		SparseBinding  = 0x00000008,
		Protected      = 0x00000010,
		VideoDecodeKhr = 0x00000020,
		VideoEncodeKhr = 0x00000040,
		FlagBitsMax    = 0x7FFFFFFF
	};
    VK_FLAG_BITS_OPERATORS_DEFINE(Queue);
	constexpr QueueFlags QFlagGCT = QueueFlagBits::Graphics | QueueFlagBits::Compute | QueueFlagBits::Transfer;
	constexpr QueueFlags QFlagC	  = +QueueFlagBits::Compute;
	constexpr QueueFlags QFlagT	  = +QueueFlagBits::Transfer;

    VK_TYPE_ALIAS_DEFINE(ShaderStageFlags);
	enum class ShaderStageFlagBits : ShaderStageFlags {
		Vertex                 = 0x00000001,
		TessellationControl    = 0x00000002,
		TessellationEvaluation = 0x00000004,
		Geometry               = 0x00000008,
		Fragment               = 0x00000010,
		Compute                = 0x00000020,
		AllGraphics            = 0x0000001f,
		All                    = 0x7fffffff,
		RaygenKhr              = 0x00000100,
		AnyHitKhr              = 0x00000200,
		ClosestHitKhr          = 0x00000400,
		MissKhr                = 0x00000800,
		IntersectionKhr        = 0x00001000,
		CallableKhr            = 0x00002000,
		TaskNv                 = 0x00000040,
		MeshNv                 = 0x00000080,
		SubpassShadingHuawei   = 0x00004000,
		RaygenNv               = RaygenKhr,
		AnyHitNv               = AnyHitKhr,
		ClosestHitNv           = ClosestHitKhr,
		MissNv                 = MissKhr,
		IntersectionNv         = IntersectionKhr,
		CallableNv             = CallableKhr,
		MaxEnum                = 0x7FFFFFFF
	};
    VK_FLAG_BITS_OPERATORS_DEFINE(ShaderStage);
#undef VK_FLAG_BITS_OPERATORS_DEFINE
#pragma endregion Enums

    VK_TYPE_ALIAS_DEFINE(Bool32);
	constexpr Bool32 VkFalse = VK_FALSE;
	constexpr Bool32 VkTrue  = VK_TRUE;

	constexpr auto null_handle       = VK_NULL_HANDLE;
	constexpr auto end_of_chain      = nullptr;
	constexpr auto default_allocator = nullptr;
    constexpr auto no_fence          = VK_NULL_HANDLE;
    constexpr auto disable_timeout   = UINT64_MAX;
    constexpr auto wait_all          = VK_TRUE;

	constexpr uint32_t MakeApiVersion(uint32_t variant, uint32_t major, uint32_t minor, uint32_t patch = 0) {
		return VK_MAKE_API_VERSION(variant, major, minor, patch);
	}
	constexpr uint32_t ApiVersionMajor(uint32_t version) {
		return VK_API_VERSION_MAJOR(version);
	}
	constexpr uint32_t ApiVersionMinor(uint32_t version) {
		return VK_API_VERSION_MINOR(version);
	}
#undef VMA_TYPE_ALIAS_DEFINE
#undef VK_TYPE_ALIAS_DEFINE

    //
    // concepts
    // 
    template<typename T>
    concept Array = requires(T a) {
        static_cast<uint32_t>(a.size());
        *(a.data());
    };
    static_assert(Array <std::vector<int>>);
    static_assert(Array <std::array<int, 3>>);
    
    template<typename T>
    concept TypeWithMemsize = requires(T a) {
        sizeof(typename T::value_type) > 0;
        static_cast<uint32_t>(a.size());
    };
	//template<typename T>
	//	requires requires(T a) {
	//		typename T::value_type;
	//		static_cast<uint32_t>(a.size());
	//	}
    template<TypeWithMemsize T>
	constexpr size_t memsize(const T& container) {
		return sizeof(T::value_type) * container.size();
	}
	template<TypeWithMemsize T, TypeWithMemsize ...Ts>
	constexpr size_t memsize(T const& container, Ts const& ...containers) {
		return sizeof(T::value_type) * container.size() + memsize(containers...);
	}
    //
    // Extension struct
    //
#pragma region Extension Structs
#define VK_CHECK_SIZE(Typename) static_assert(sizeof(Typename) == sizeof(Vk##Typename), "Memory layout of " #Typename " is not matched with Vk" #Typename "!")
    struct MemoryBarrier2KHR {
        VkStructureType             sType{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR};
        const void*                 pNext{end_of_chain};
        VkPipelineStageFlags2KHR    srcStageMask;
        VkAccessFlags2KHR           srcAccessMask;
        VkPipelineStageFlags2KHR    dstStageMask;
        VkAccessFlags2KHR           dstAccessMask;
    };
    VK_CHECK_SIZE(MemoryBarrier2KHR);
    struct BufferMemoryBarrier2KHR {
        VkStructureType             sType{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR};
        const void*                 pNext{end_of_chain};
        VkPipelineStageFlags2KHR    srcStageMask;
        VkAccessFlags2KHR           srcAccessMask;
        VkPipelineStageFlags2KHR    dstStageMask;
        VkAccessFlags2KHR           dstAccessMask;
        uint32_t                    srcQueueFamilyIndex;
        uint32_t                    dstQueueFamilyIndex;
        VkBuffer                    buffer;
        VkDeviceSize                offset;
        VkDeviceSize                size;
    };
    VK_CHECK_SIZE(BufferMemoryBarrier2KHR);
    struct ImageMemoryBarrier2KHR {
        VkStructureType             sType{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR};
        const void*                 pNext{end_of_chain};
        VkPipelineStageFlags2KHR    srcStageMask;
        VkAccessFlags2KHR           srcAccessMask;
        VkPipelineStageFlags2KHR    dstStageMask;
        VkAccessFlags2KHR           dstAccessMask;
        VkImageLayout               oldLayout;
        VkImageLayout               newLayout;
        uint32_t                    srcQueueFamilyIndex;
        uint32_t                    dstQueueFamilyIndex;
        VkImage                     image;
        VkImageSubresourceRange     subresourceRange;
    };
    VK_CHECK_SIZE(ImageMemoryBarrier2KHR);
    struct DependencyInfoKHR {
        VkStructureType                  sType{VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR};
        const void*                      pNext{end_of_chain};
        VkDependencyFlags                dependencyFlags;
        uint32_t                         memoryBarrierCount;
        const VkMemoryBarrier2KHR*       pMemoryBarriers;
        uint32_t                         bufferMemoryBarrierCount;
        const VkBufferMemoryBarrier2KHR* pBufferMemoryBarriers;
        uint32_t                         imageMemoryBarrierCount;
        const VkImageMemoryBarrier2KHR*  pImageMemoryBarriers;
    };
    VK_CHECK_SIZE(DependencyInfoKHR);

	struct CopyImageInfo2KHR {
		VkStructureType sType{VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2_KHR};
		const void*     pNext{end_of_chain};
		VkImage         srcImage;
		VkImageLayout   srcImageLayout;
		VkImage         dstImage;
		VkImageLayout   dstImageLayout;
		uint32_t        regionCount;
		const VkImageCopy2KHR* pRegions;
	};
    VK_CHECK_SIZE(CopyImageInfo2KHR);

    struct BlitImageInfo2KHR {
        VkStructureType        sType{VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2_KHR};
        const void*            pNext{end_of_chain};
        VkImage                srcImage;
        VkImageLayout          srcImageLayout;
        VkImage                dstImage;
        VkImageLayout          dstImageLayout;
        uint32_t               regionCount;
        const VkImageBlit2KHR* pRegions;
        VkFilter               filter;
    };
    VK_CHECK_SIZE(BlitImageInfo2KHR);

#pragma endregion Extension Structs

	//
	// Format:
	// 
	// LunarG Validation Layer
	//   	|
	//   	|---[Layer Name]--> VK_LAYER_LUNARG_device_limits
	//   			|
	//   			|---[Layer Extension]--> VK_EXT_debug_report
	// 
	struct LayerProperty {
		VkLayerProperties property;
		std::vector<VkExtensionProperties> extensions;
		//VkResult getExtensionProperties(std::vector<VkPhysicalDevice> gpus = {});
		VkResult getExtensionProperties(GpuList gpus = {});
	};

	// TODO: support optional layers and extensions
	class LayerAndExtension {
		std::vector<LayerProperty> layerPropertyList;
		NameList requiredLayerNames;
		NameList requiredExtensionNames;
		//NameList requiredFeatureNames;
	public:
		NameList enabledLayerNames;
		NameList enabledExtensionNames;
		//NameList enabledFeatureNames;
		VK_DELETE_ALL_DEFAULT(LayerAndExtension);
		explicit LayerAndExtension(NameList requiredExtensionNames_)
			: requiredExtensionNames(std::move(requiredExtensionNames_)) {}
		LayerAndExtension(NameList requiredLayerNames_, NameList requiredExtensionNames_)
			: requiredLayerNames(std::move(requiredLayerNames_)), requiredExtensionNames(std::move(requiredExtensionNames_)) {}
		//LayerAndExtension(NameList requiredLayerNames_, NameList requiredExtensionNames_, NameList requiredFeatureNames_)
		//	: requiredLayerNames(std::move(requiredLayerNames_)), requiredExtensionNames(std::move(requiredExtensionNames_)), requiredFeatureNames(std::move(requiredFeatureNames_)) {}

		operator std::vector<LayerProperty>() const { return layerPropertyList; }

		VkResult getInstanceLayerProperties();
		VkResult getInstanceExtensionProperties();
		void	 initEnabledInstanceLayerAndExtensions();
		VkResult getDeviceExtensionProperties(const LayerAndExtension& instanceLayerExtension, GpuList gpus);

		// TODO: select the device that support all required extensions
		bool	 initEnabledDeviceExtensions(VkPhysicalDevice gpu);
	};

	struct InstanceCreateInfo {
		const char* appName;
		const char* engineName;
		uint32_t    apiMajor{1};
		uint32_t    apiMinor{2};
		NameList    requiredLayerNames;
		NameList    requiredExtensionNames;
        void*       pRequiredFeatures{end_of_chain};

		bool validationEnabled{false};
		void enableValidationLayer() {
			if (!validationEnabled) {
				requiredLayerNames.push_back("VK_LAYER_KHRONOS_validation");
				requiredExtensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
				validationEnabled = true;
			}
		}
	};

	class Instance {
		VkInstance m_instance{null_handle};
		LayerAndExtension layerExtensions;
		bool validationEnabled;
        uint32_t ApiVersion;

	// load instance extension functions
    #if !__has_include(<volk.h>)
		//VK_DECLARE(vkCreateDebugReportCallbackEXT);
		//VK_DECLARE(vkDestroyDebugReportCallbackEXT);
		VK_DECLARE(vkCreateDebugUtilsMessengerEXT);
		VK_DECLARE(vkDestroyDebugUtilsMessengerEXT);
	public:
		void loadExtensionFunctions() {
            throw std::runtime_error(fmt::format("Some extension functions are not loaded, in {}: Line {}", __FILE__, __LINE__));
			//VK_LOAD_INSTANCE_EXT_FUNCTION(m_instance, vkCreateDebugReportCallbackEXT);
			//VK_LOAD_INSTANCE_EXT_FUNCTION(m_instance, vkDestroyDebugReportCallbackEXT);
			if (validationEnabled) {
				VK_LOAD_INSTANCE_EXT_FUNCTION(m_instance, vkCreateDebugUtilsMessengerEXT);
				VK_LOAD_INSTANCE_EXT_FUNCTION(m_instance, vkDestroyDebugUtilsMessengerEXT);
			}
		}
	#endif
	// debug utilities
	#if 1
	private:
		VkDebugUtilsMessengerEXT m_debugMessenger;
		static VKAPI_ATTR VkBool32 VKAPI_CALL debugMessengerCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT		messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT				messageType,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void*										pUserData) {
			// TODO: use more colors for different severities
			// TODO: report message type
			if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
				LogError("\n");
				LogError("Validation ERROR: {}\n", pCallbackData->pMessage);
			}
			else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
				LogWarning("\n");
				LogWarning("Validation WARNING: {}\n", pCallbackData->pMessage);
			}
			else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
				LogInfo("\n");
				LogInfo("Validation INFO: {}\n", pCallbackData->pMessage);
			} else {
				LogInfo("\n");
				LogInfo("Validation VERBOSE: {}\n", pCallbackData->pMessage);
			}
			return VK_FALSE;
		}
		void initDebugUtils();
		
	#endif
	public:
		VK_DELETE_ALL_DEFAULT(Instance);

		void checkInstanceLayersAndExtensions() {
			layerExtensions.getInstanceLayerProperties() | VK_NO_ERROR;
			layerExtensions.getInstanceExtensionProperties() | VK_NO_ERROR;
			layerExtensions.initEnabledInstanceLayerAndExtensions();
		}

		explicit Instance(const InstanceCreateInfo& info);

		~Instance() {
#if !__has_include(<volk.h>)
			m_vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, default_allocator);
#else
			vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, default_allocator);
#endif
			m_debugMessenger = null_handle;
			if (m_instance != null_handle) {
				vkDestroyInstance(m_instance, default_allocator);
				m_instance = null_handle;
			}
		}

		operator VkInstance() const { return m_instance; }

        [[nodiscard]]
		LayerAndExtension const& getLayerExtensions() const { return layerExtensions; }

        [[nodiscard]]
        uint32_t getApiVersion() const { return ApiVersion; }
	};

    //
    // Feature wrappers
    // usage:
    //      std::vector<*PhysicalDeviceFeature> featureList{...}; // input
    //      gpuFeatures12.pNext = featureList[0]->ptr();
    //      for (auto i = 0uz; i < featureList.size()-1; ++i) {
    //          featureList[i]->pNext() = featureList[i+1]->ptr();
    //      }
    //
    struct PhysicalDeviceFeature {
        [[nodiscard]] virtual bool        enabled() const = 0;
        [[nodiscard]] virtual void*       ptr()   = 0;
        [[nodiscard]] virtual void*&      pNext() = 0;
        [[nodiscard]] virtual const char* name()  = 0;
    };
    struct NullFeature : public PhysicalDeviceFeature {
        using cType = std::nullptr_t;
        void* feature_ptr {nullptr};
        [[nodiscard]] bool enabled() const override {
            return false;
        }
        [[nodiscard]] void* ptr() override {
            return nullptr;
        }
        [[nodiscard]] void*& pNext() override {
            return feature_ptr;
        }
        [[nodiscard]] const char* name() override {
            return "nullFeature";
        }
    };
	struct BufferDeviceAddressFeatures : public PhysicalDeviceFeature {
		using cType = VkPhysicalDeviceBufferDeviceAddressFeatures;
		cType m_feature{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
			.pNext = nullptr,
		};
		[[nodiscard]] bool enabled() const override {
			return m_feature.bufferDeviceAddress == VK_TRUE;
		}
		[[nodiscard]] void* ptr() override {
			return static_cast<void*>(&m_feature);
		}
		[[nodiscard]] void*& pNext() override {
			return m_feature.pNext;
		}
		[[nodiscard]] const char* name() override {
			return "bufferDeviceAddress";
		}
	};

	struct Synchronization2FeaturesKHR : public PhysicalDeviceFeature {
		using cType = VkPhysicalDeviceSynchronization2FeaturesKHR;
		cType m_feature{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
			.pNext = nullptr,
		};
		[[nodiscard]] bool enabled() const override {
			return m_feature.synchronization2 == VK_TRUE;
		}
		[[nodiscard]] void* ptr() override {
			return static_cast<void*>(&m_feature);
		}
		[[nodiscard]] void*& pNext() override {
			return m_feature.pNext;
		}
		[[nodiscard]] const char* name() override {
			return "synchronization2KHR";
		}
	};

    struct RayTracingPipelineKHR : public PhysicalDeviceFeature {
        using cType = VkPhysicalDeviceRayTracingPipelineFeaturesKHR;
        cType m_feature {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
            .pNext = nullptr,
        };
        [[nodiscard]] bool enabled() const override {
            return m_feature.rayTracingPipeline == VK_TRUE;
        }
        [[nodiscard]] void* ptr() override {
            return static_cast<void*>(&m_feature);
        }
        [[nodiscard]] void*& pNext() override {
            return m_feature.pNext;
        }
        [[nodiscard]] const char* name() override {
            return "rayTracingPipelineKHR";
        }
    };
    struct RayQueryFeaturesKHR : public PhysicalDeviceFeature {
        using cType = VkPhysicalDeviceRayQueryFeaturesKHR;
        cType m_feature {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
            .pNext = nullptr,
        };
        [[nodiscard]] bool enabled() const override {
            return m_feature.rayQuery == VK_TRUE;
        }
        [[nodiscard]] void* ptr() override {
            return static_cast<void*>(&m_feature);
        }
        [[nodiscard]] void*& pNext() override {
            return m_feature.pNext;
        }
        [[nodiscard]] const char* name() override {
            return "rayQueryKHR";
        }
    };
    struct AccelerationStructureFeaturesKHR : public PhysicalDeviceFeature {
        using cType = VkPhysicalDeviceAccelerationStructureFeaturesKHR;
        cType m_feature {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
                .pNext = nullptr,
        };
        [[nodiscard]] bool enabled() const override {
            return m_feature.accelerationStructure == VK_TRUE;
        }
        [[nodiscard]] void* ptr() override {
            return static_cast<void*>(&m_feature);
        }
        [[nodiscard]] void*& pNext() override {
            return m_feature.pNext;
        }
        [[nodiscard]] const char* name() override {
            return "accelerationStructureKHR";
        }
    };
    struct MeshShaderFeaturesNV : public PhysicalDeviceFeature {
        using cType = VkPhysicalDeviceMeshShaderFeaturesNV;
        cType m_feature {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV,
                .pNext = nullptr,
        };
        [[nodiscard]] bool enabled() const override {
            return m_feature.meshShader == VK_TRUE && m_feature.taskShader == VK_TRUE;
        }
        [[nodiscard]] void* ptr() override {
            return static_cast<void*>(&m_feature);
        }
        [[nodiscard]] void*& pNext() override {
            return m_feature.pNext;
        }
        [[nodiscard]] const char* name() override {
            return "meshShaderNV";
        }

    };
	//
	// Manage GPUs and corresponding information (properties, memory properties, features)
	// TODO: add DeviceGroup support
	//
    struct DeviceCreateInfo;
	class PhysicalDevice {
	public:
        struct Checker {
			std::function<bool(VkPhysicalDevice)> check;
			const char* info;
			std::vector<VkBool32> result;
		};

	private:
		VkPhysicalDevice m_gpu;

		GpuList	gpuList;
		std::vector<Checker> gpuCheckerList;

        VkPhysicalDeviceMeshShaderPropertiesNV meshShaderProperties{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_NV,
            .pNext = end_of_chain,
        };
		VkPhysicalDeviceVulkan12Properties gpuProperties12{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES,
			.pNext = end_of_chain,
		};
		VkPhysicalDeviceVulkan11Properties gpuProperties11{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES,
			.pNext = &gpuProperties12,
		};
		VkPhysicalDeviceProperties2 gpuProperties{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
			.pNext = &gpuProperties11,
		};

		VkPhysicalDeviceMemoryProperties2 gpuMemoryProperties{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
		};


        std::vector<void*> additionalGpuFeatures;
		// TODO: features
		VkPhysicalDeviceVulkan12Features gpuFeatures12{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
			.pNext = end_of_chain
		};
		VkPhysicalDeviceVulkan11Features gpuFeatures11{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
			.pNext = &gpuFeatures12
		};
		VkPhysicalDeviceFeatures2 gpuFeatures{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
			.pNext = &gpuFeatures11
		};

	public:
		std::vector<VkQueueFamilyProperties> queueFamilyPropsList;

		VK_DELETE_ALL_DEFAULT(PhysicalDevice);
		// After the constructor, layerExtensions contains required device extensions and available extensions
		PhysicalDevice(Instance& instance_, LayerAndExtension& layerExtensions, IN OUT DeviceCreateInfo& info);

        [[nodiscard]]
		VkPhysicalDeviceFeatures2 const* features() const { return &gpuFeatures; }
        [[nodiscard]]
		void* additionalFeatures() { return gpuFeatures12.pNext; }
		operator VkPhysicalDevice() const { return m_gpu; }
        [[nodiscard]]
		const GpuList& getGpuList() const { return gpuList; }

        [[nodiscard]]
        VkPhysicalDeviceMemoryProperties const& MemoryProperties() const {
            return gpuMemoryProperties.memoryProperties;
        }

        [[nodiscard]]
        const auto& properties() const {
            return gpuProperties.properties;
        }
	};

    class Device;
    // handle to queue
	struct Queue {
        Device const* m_device;
		VkQueue  m_queue    { null_handle };
		uint32_t queueIndex { ~0u };
		uint32_t familyIndex{ ~0u };
		
		operator VkQueue() const { return m_queue; }

        void Submit(uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence) {
            vkQueueSubmit(m_queue, submitCount, pSubmits, fence) | VK_NO_ERROR;
        }
        void Submit2KHR() {

        }
	};

	

	struct DeviceCreateInfo {
		NameList requiredLayerNames; // not used
		NameList requiredExtensionNames;
        std::vector<PhysicalDeviceFeature*> requiredFeatures;
        std::vector<PhysicalDeviceFeature*> optionalFeatures;
		std::vector<PhysicalDevice::Checker> checkers;
	};
	//
	// Physical device + logical device + queue families of physical devices
	// Currently only single GPU support
	//
	class Device {
		Instance& m_instance;
        const VkAllocationCallbacks* m_allocator;
		LayerAndExtension layerExtensions;

		PhysicalDevice gpu;

		struct QueueInfo {
			uint32_t score;
			uint32_t queueIndex;
			uint32_t familyIndex;
			auto operator<=>(const QueueInfo&) const = default;
		};
		std::vector<QueueInfo> unusedQueues;

		VkDevice m_device{ null_handle };

	public:
		Queue m_queueGCT;
        Queue m_queueCT;
		Queue m_queueT;

		Queue m_queuePresent;

	private:
		Queue takeQueue(QueueFlags flags);

		static void isValidQueue(const Queue& queue) {
			if (queue.m_queue == null_handle || queue.familyIndex == ~0u || queue.queueIndex == ~0u)
				throw std::runtime_error("Error: invalid queue!\n");
		}

	public:
		VK_DELETE_ALL_DEFAULT(Device);
		operator VkDevice() const { return m_device; }
		operator VkPhysicalDevice() const { return gpu; }
		Device(DeviceCreateInfo& info, Instance& instance_, const VkAllocationCallbacks* allocator = default_allocator);
		~Device() {
			if (m_device != null_handle) {
                WaitIdle();
				vkDestroyDevice(m_device, m_allocator);
				m_device = null_handle;
			}
		}

        [[nodiscard]]
        auto allocator() const {
            return m_allocator;
        }

        void WaitIdle() const {
            vkDeviceWaitIdle(m_device) | VK_NO_ERROR;
        }

        [[nodiscard]]
        VkPhysicalDeviceMemoryProperties MemoryProperties() const {
            return gpu.MemoryProperties();
        }

        [[nodiscard]]
        uint32_t ApiVersion() const {
            return m_instance.getApiVersion();
        }

        [[nodiscard]]
        Instance& getInstance() {
            return m_instance;
        }

        [[nodiscard]]
        const auto& getProperties() const {
            return gpu.properties();
        }

	// extension specific functions
#ifdef VOLK_DEVICE_TABLE_ENABLED
    private:
        VolkDeviceTable table;
    public:

#endif
#if !__has_include(<volk.h>)
	private:
		VK_DECLARE(vkBuildAccelerationStructuresKHR);
        VK_DECLARE(vkCmdDrawMeshTasksNV);
        VK_DECLARE(vkCmdPipelineBarrier2KHR);
	public:
		void loadExtensionFunctions() {
			VK_LOAD_DEVICE_EXT_FUNCTION(m_device, vkBuildAccelerationStructuresKHR);
            VK_LOAD_DEVICE_EXT_FUNCTION(m_device, vkCmdDrawMeshTasksNV);
            VK_LOAD_DEVICE_EXT_FUNCTION(m_device, vkCmdPipelineBarrier2KHR);
		}
		VkResult BuildAccelerationStructures (
			VkDeferredOperationKHR								   deferredOperation,
			uint32_t											   infoCount,
			const VkAccelerationStructureBuildGeometryInfoKHR*	   pInfos,
			const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos)
		const {

			throw std::runtime_error("Not implemented yet");
			return m_vkBuildAccelerationStructuresKHR(m_device, deferredOperation, infoCount, pInfos, ppBuildRangeInfos);
		}
		void CmdDrawMeshTasksNV(VkCommandBuffer commandBuffer, uint32_t taskCount, uint32_t firstTask)
		const {
            m_vkCmdDrawMeshTasksNV(commandBuffer, taskCount, firstTask);
        }
        void CmdPipelineBarrier2KHR(
            VkCommandBuffer          commandBuffer,
            DependencyInfoKHR const& dependencyInfo)
        const {
            m_vkCmdPipelineBarrier2KHR(commandBuffer, reinterpret_cast<const VkDependencyInfoKHR*>(&dependencyInfo));
        }
#endif
        
		void checkSurfaceSupportAndSetPresentQueue(VkSurfaceKHR surface, bool raytracing);
		
	};

	// 
	// SurfaceKHR
	//
	// optional Ins/Outs are used to create swapchain
	// 
	// input:
	//		instance, surface, (glfw window) width, height
	//		optional:
	//			VkPhysicalDevice
	// output:
	//		PhysicalDevice::Checker (check swapchain support)
	//		optional:
	//			SwapchainSurfaceFormat
	//			SwapchainPresentMode
	//			SwapchainExtent
	//			
	// 

	struct SwapchainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities{};
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	//static_assert(sizeof(VkSurfaceKHR) == sizeof(void*));
	class Surface {
		Instance&	 m_instance;
		VkSurfaceKHR m_surface{null_handle};
		SwapchainSupportDetails supportDetails;
		int width;
		int height;
	public:
		VK_DELETE_ALL_DEFAULT(Surface);
		operator VkSurfaceKHR() const { return m_surface; }

	// input:
	//		instance, surface, (glfw window) width, height
	// output:
	//		PhysicalDevice::Checker (check swapchain support)
		//hopefully the validation layer could detect whether the original type of void* surface_ is incorrect
		Surface(Instance& instance_, void* surface_, OUT std::vector<PhysicalDevice::Checker>& checkers, int windowWidth, int windowHeight) :
			m_instance(instance_), m_surface(static_cast<VkSurfaceKHR>(surface_)), width(windowWidth), height(windowHeight)
		{
			checkers.push_back(
				PhysicalDevice::Checker{
					.check = [this](VkPhysicalDevice gpu) {
						this->fillSwapchainSupportDetails(gpu);
						return !supportDetails.formats.empty() && !supportDetails.presentModes.empty();
					},
					.info = "Swapchain Support"
				}
			);
		}
		~Surface() {
            if (m_surface != null_handle) {
			    vkDestroySurfaceKHR(m_instance, m_surface, default_allocator);
                m_surface = null_handle;
            }
		}
		VkSurfaceKHR* data() { return &m_surface; }

	// input:
	//		optional:
	//			VkPhysicalDevice
		// TODO: add a read-only version
		//SwapchainSupportDetails getSupportDetails(VkPhysicalDevice gpu) const { SwapchainSupportDetails supportDetails; return supportDetails; }
        SwapchainSupportDetails& fillSwapchainSupportDetails(VkPhysicalDevice gpu);

        [[nodiscard]]
		auto&		getSwapchainSupportDetails()	   { return supportDetails; }
		[[nodiscard]]
		const auto& getSwapchainSupportDetails() const { return supportDetails; }

	// output:
	//		optional:
	//			SwapchainSurfaceFormat
	//			SwapchainPresentMode
	//			SwapchainExtent
		VkSurfaceFormatKHR chooseSwapchainSurfaceFormat();
		VkPresentModeKHR   chooseSwapchainPresentMode();
		VkExtent2D         chooseSwapchainExtent();

	};

	// for RAII resources
#define VK_NO_COPY_NO_DEFAULT(Typename) \
		Typename() = delete;\
		Typename(Typename const&) = delete;\
		Typename& operator=(Typename const&) = delete

//#define VK_NO_COPY(Typename) \
//		Typename(Typename const&) = delete;\
//		Typename& operator=(Typename const&) = delete

//#define VK_CHECK_SIZE(Typename) static_assert(sizeof(Typename) == sizeof(Vk##Typename), "Memory layout of " #Typename " is not matched with Vk" #Typename "!")

	// TODO: check the bytecode size and make sure the performance of io streams is acceptable
	// TODO: dynamically compile glsl
	namespace fs = std::filesystem;
	std::string LoadFile(in_t<fs::path> filepath, bool binary = true) {
        std::ios::openmode openmode = std::ios::in;
        if (binary) openmode = openmode | std::ios::binary;
		std::ifstream file(filepath, openmode);
		if (not file.is_open()) {
			throw std::runtime_error(fmt::format("Cannot load {}", filepath.string()));
		}
		std::stringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
	}

#define VK_TEST_READ_FILE
#ifdef VK_TEST_READ_FILE
	std::vector<char> readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file!");
        }

        auto fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }
#endif

	//
	// ShaderSPIRV
	// input:
	//		none -> vertex shader and fragment shader
	//		name of shader source files
	// output:
	//		load compiled SPIR-V bytecodes
	//
	struct ShaderSPIRV {
		std::string vertexShaderCode;
		std::string fragmentShaderCode;
		std::unordered_map<std::string, std::string> shaderCodes;
#ifdef VK_TEST_READ_FILE
		static void testfile(const std::string& s1, const std::vector<char>& s2) {
			if (s1.length() != s2.size()) {
				throw std::runtime_error("not matched!");
			}
			for (auto i = 0uz; i < s1.length(); ++i) {
				if (s1[i] != s2[i]) {
					throw std::runtime_error("not matched!");
				}
			}
		}
#endif
		ShaderSPIRV() {
			fs::path pathToVertexShader   = "Shaders/shader.vert.spv";
			fs::path pathToFragmentShader = "Shaders/shader.frag.spv";
			LogInfo("Loading SPIR-V shaders: {}\n", pathToVertexShader.string());
			vertexShaderCode   = LoadFile(pathToVertexShader);
			LogInfo("Loading SPIR-V shaders: {}\n", pathToFragmentShader.string());
			fragmentShaderCode = LoadFile(pathToFragmentShader);

            std::unordered_map<std::string, fs::path> pathToShaders;
            pathToShaders["Mesh"] = "Shaders/hardwired_triangle.mesh.spv";
            pathToShaders["Frag"] = "Shaders/hardwired_triangle.frag.spv";
            for (auto& [key, value] : pathToShaders) {
                LogInfo("Loading SPIR-V shaders: {}\n", pathToShaders[key].string());
                shaderCodes[key] = LoadFile(value);
            }

#ifdef VK_TEST_READ_FILE
			auto test_vert = readFile("Shaders/shader.vert.spv");
			auto test_frag = readFile("Shaders/shader.frag.spv");
			testfile(vertexShaderCode, test_vert);
			testfile(fragmentShaderCode, test_frag);
            LogWarning("\n");
            LogWarning("VK_TEST_READ_FILE passed!\n");
            LogWarning("\n");
#endif
		}
		explicit ShaderSPIRV(std::unordered_map<std::string, std::string>& shaderSources) {//: shaderCodes(shaderSources.size()) {...}
            for (auto& [key, value] : shaderSources) {
                fs::path pathToShader = std::string("Shaders/") + value + ".spv";
                LogInfo("Loading SPIR-V shaders: {}\n", pathToShader.string());
                shaderCodes[key] = LoadFile(pathToShader);
            }
		}
		//VkShaderModule to_VkShaderModule(const std::string& code) {

	    //}
    };


	//
	// ShaderModule
	// input:
	//		const std::string& bytecode
	// output:
	//		RAII class ShaderModule
	//

	class ShaderModule {
	// valid state: m_shaderModule is either null_handle or something created with m_device and m_allocator
		VkDevice m_device;
		VkShaderModule m_shaderModule;
		const VkAllocationCallbacks* m_allocator;
	public:
		VK_NO_COPY_NO_DEFAULT(ShaderModule);
		ShaderModule(in_t<Device> device, in_t<std::string> bytecode, const VkAllocationCallbacks* allocator = default_allocator) :
		    m_device(device), m_shaderModule(), m_allocator(allocator)
        {
			VkShaderModuleCreateInfo createInfo{
				.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
				.codeSize = bytecode.size(),
				.pCode    = reinterpret_cast<const uint32_t*>(bytecode.data())
			};
			vkCreateShaderModule(m_device, &createInfo, m_allocator, &m_shaderModule) | VK_NO_ERROR;
		}

		ShaderModule(Device const& device, VkShaderModule shaderModule, const VkAllocationCallbacks* allocator = default_allocator) :
			m_device(device), m_shaderModule(shaderModule), m_allocator(allocator) {}

		ShaderModule(ShaderModule&& shaderModule_tm) noexcept :
                m_device(shaderModule_tm.m_device), m_shaderModule(std::exchange(shaderModule_tm.m_shaderModule, null_handle)), m_allocator(shaderModule_tm.m_allocator) {}

		ShaderModule& operator=(ShaderModule&& shaderModule_tm) noexcept {
			if (this != &shaderModule_tm) {
				ShaderModule tmp(std::move(shaderModule_tm));
				this->swap(tmp);
			}
			return *this;
		}

		void swap(ShaderModule& shaderModule_ts) noexcept {
			std::swap(m_device      , shaderModule_ts.m_device      );
			std::swap(m_shaderModule, shaderModule_ts.m_shaderModule);
			std::swap(m_allocator   , shaderModule_ts.m_allocator   );
		}

		~ShaderModule() {
			if (m_shaderModule != null_handle) {
				vkDestroyShaderModule(m_device, m_shaderModule, m_allocator);
                m_shaderModule = null_handle;
			}
		}

        operator VkShaderModule() const { return m_shaderModule; }
	};


//#undef VK_ENABLE_RESOURCE_ALLOCATOR
#define VK_ENABLE_RESOURCE_ALLOCATOR
	//
	// ResourceAllocator
	// allocate VkBuffer and VkImage
	//
#ifdef VK_ENABLE_RESOURCE_ALLOCATOR

	struct BufferCreateInfo {
        VkStructureType     sType{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        const void*         pNext{end_of_chain};
        VkBufferCreateFlags flags{};
        DeviceSize          size {};
        BufferUsageFlags    usage{};
        VkSharingMode       sharingMode{VK_SHARING_MODE_EXCLUSIVE};
        uint32_t            queueFamilyIndexCount{1u};
        const uint32_t*     pQueueFamilyIndices{};
	};
    VK_CHECK_SIZE(BufferCreateInfo);

    class BufferVma;
    struct BufferVmaCreateInfo {
        BufferCreateInfo bufferCreateInfo;
        VmaAllocationCreateInfo allocationCreateInfo;
    };



	struct ImageCreateInfo {
		VkStructureType          sType				   {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
		const void*              pNext				   {};
		VkImageCreateFlags       flags				   {};
		VkImageType              imageType			   {VK_IMAGE_TYPE_2D};
		VkFormat                 format				   {};
		VkExtent3D               extent				   {};
		uint32_t                 mipLevels			   {1u};
		uint32_t                 arrayLayers		   {1u};
		VkSampleCountFlagBits    samples			   {VK_SAMPLE_COUNT_1_BIT};
		VkImageTiling            tiling				   {};
		VkImageUsageFlags        usage				   {};
		VkSharingMode            sharingMode		   {VK_SHARING_MODE_EXCLUSIVE};
		uint32_t                 queueFamilyIndexCount {1u};
		const uint32_t*          pQueueFamilyIndices   {};
		VkImageLayout            initialLayout		   {VK_IMAGE_LAYOUT_UNDEFINED};
	};
	VK_CHECK_SIZE(ImageCreateInfo);

    class ImageVma;
    struct ImageVmaCreateInfo {
        ImageCreateInfo imageCreateInfo;
        VmaAllocationCreateInfo allocationCreateInfo;
    };

	//class Buffer;
	//class Image;
	//class ImageCreateInfo;
	class ResourceAllocator {
		Device& m_device;
        VmaAllocator m_vmaAllocator;
		//VkAllocationCallbacks* allocator{default_allocator};
	public:
		VK_DELETE_ALL_DEFAULT(ResourceAllocator);
        explicit ResourceAllocator(Device& device, VmaAllocationCreateFlags flags = 0) : m_device{device}, m_vmaAllocator{ nullptr } {
            LogWarning("Initializing Vulkan Resource Allocator...\n");
            if (m_device.ApiVersion() != VK_API_VERSION_1_2) {
                LogWarning("Current API Version: [{}]. The resource allocator is only tested with Vulkan 1.2 [{}]\n", m_device.ApiVersion(), VK_API_VERSION_1_2);
            }
            VmaAllocatorCreateInfo createInfo{
                .flags            = flags,
                .physicalDevice   = static_cast<VkPhysicalDevice>(m_device),
                .device           = static_cast<VkDevice>(m_device),
                .instance         = m_device.getInstance(),
                .vulkanApiVersion = m_device.ApiVersion(),
            };
            vmaCreateAllocator(&createInfo, &m_vmaAllocator) | VK_NO_ERROR;
        }

        ~ResourceAllocator() {
			if (m_vmaAllocator != nullptr) {
				vmaDestroyAllocator(m_vmaAllocator);
			}
        }

        explicit operator VmaAllocator() const { return m_vmaAllocator; }
		explicit operator VkDevice() const { return static_cast<VkDevice>(m_device); }

        Device& device() { return m_device; }
        Device const& device() const { return m_device; }

        template<typename VertexType>
        BufferVma CreateVertexBuffer(std::vector<VertexType> const& vertices) {
            throw std::runtime_error("Error in vk::ResourceAllocator::CreateVertexBuffer: not implemented yet!");
            if (vertices.empty()) {
                LogError("Vulkan error: trying to create empty vertex buffer!");
                throw std::runtime_error("Vulkan error: trying to create empty vertex buffer!");
            }
        }

        [[nodiscard]]
        BufferVma CreateBufferVma(in_t<BufferVmaCreateInfo> info);

		[[nodiscard]]
        ImageVma CreateImageVma(in_t<ImageVmaCreateInfo> info);
	};

    template<typename T>
    concept DataContainer = requires(T a) {
        typename T::value_type;
		static_cast<uint32_t>(a.size());
		*(a.data());
	};
    static_assert(DataContainer<std::vector<float>>);

    auto PaddingSize(std::integral auto size, std::integral auto alignment) {
        return (alignment > 0 && size % alignment) ? (alignment - size % alignment) : 0;
    }

    //
    // BufferVma
    //
    class BufferVma {
        VkBuffer m_buffer;
        VmaAllocation m_vmaAllocation;
        ResourceAllocator& m_allocator;
    public:
        VK_NO_COPY_NO_DEFAULT(BufferVma);
        explicit BufferVma(ResourceAllocator& allocator) :
            m_buffer{null_handle}, m_vmaAllocation{null_handle}, m_allocator{allocator} {}
        BufferVma(VkBuffer buffer, VmaAllocation vmaAllocation, ResourceAllocator& allocator) :
            m_buffer{buffer}, m_vmaAllocation{vmaAllocation}, m_allocator{allocator} {}

        BufferVma(BufferVma&& buffer_tm) noexcept :
                m_buffer        { std::exchange(buffer_tm.m_buffer, null_handle)        },
                m_vmaAllocation { std::exchange(buffer_tm.m_vmaAllocation, null_handle) },
                m_allocator     { buffer_tm.m_allocator                                 }
        {
        }

        BufferVma& operator=(BufferVma&& buffer_tm) noexcept {
            if (this != &buffer_tm) {
                BufferVma tmp(std::move(buffer_tm));
                this->swap(tmp);
            }
            return *this;
        }

        void swap(BufferVma& buffer_ts) noexcept {
            if (&m_allocator != &buffer_ts.m_allocator) {
                LogError("Vulkan error: trying to swap buffers created by different allocators!\n");
            }
            std::swap(m_buffer       , buffer_ts.m_buffer       );
            std::swap(m_vmaAllocation, buffer_ts.m_vmaAllocation);
        }

        VkBuffer* buffer_ptr()    { return &m_buffer; }
        operator VkBuffer() const { return m_buffer; }
        ~BufferVma() {
            if (m_vmaAllocation != null_handle || m_buffer != null_handle) {
                vmaDestroyBuffer(static_cast<VmaAllocator>(m_allocator), m_buffer, m_vmaAllocation);
                m_vmaAllocation = null_handle;
                m_buffer = null_handle;
            }
        }

        ResourceAllocator& allocator() { return m_allocator; }

        void MapMemory(void** data) {
            vmaMapMemory(static_cast<VmaAllocator>(m_allocator), m_vmaAllocation, data) | VK_NO_ERROR;
        }
        void UnmapMemory() {
            vmaUnmapMemory(static_cast<VmaAllocator>(m_allocator), m_vmaAllocation);
        }
        void LoadData(const void* source, size_t size) {
            void* data;
            MapMemory(&data);
            memcpy(data, source, size);
            UnmapMemory();
        }

#if 0
        template<DataContainer T>
        void LoadData(T& dataContainer) {
            LoadData(dataContainer.data(), sizeof(T::value_type) * dataContainer.size());
        }

        template<DataContainer T1, DataContainer T2>
        void LoadData(T1& dataContainer1, T2& dataContainer2) {
            void* data1;
            MapMemory(&data1);
            memcpy(data1, dataContainer1.data(), memsize(dataContainer1));
            auto data2 = static_cast<T1::value_type*>(data1);
            std::advance(data2, dataContainer1.size());
            memcpy(data2, dataContainer2.data(), memsize(dataContainer2));
            UnmapMemory();
        }

        template<DataContainer T1, DataContainer T2, DataContainer T3>
        void LoadData(T1 const& dataContainer1, T2 const& dataContainer2, T3 const& dataContainer3) {
            void* data1;
            MapMemory(&data1);
            memcpy(data1, dataContainer1.data(), memsize(dataContainer1));
            auto data2 = static_cast<T1::value_type*>(data1);
            std::advance(data2, dataContainer1.size());
            memcpy(data2, dataContainer2.data(), memsize(dataContainer2));
            auto data3 = reinterpret_cast<T2::value_type*>(data2);
            std::advance(data3, dataContainer2.size());
            memcpy(data3, dataContainer3.data(), memsize(dataContainer3));
            UnmapMemory();
        }
#endif

        void LoadData(void* data) { }

        template<DataContainer T, DataContainer ...Ts>
        void LoadData(void* data, T dataContainer, Ts ...dataContainers) {
            memcpy(data, dataContainer.data(), memsize(dataContainer));
            auto data_next = static_cast<T::value_type*>(data);
            std::advance(data_next, dataContainer.size());
            LoadData(data_next, dataContainers...);
        }

        template<DataContainer ...Ts>
        void LoadData(Ts ...dataContainers) {
            void* data;
            MapMemory(&data);
            LoadData(data, dataContainers...);
            UnmapMemory();
        }

        //template<DataContainer T1, DataContainer T2>
        //void LoadDataWithAlignment(DeviceSize alignment, T1& dataContainer1, T2& dataContainer2) {
        //    void* data1;
        //    auto size1 = memsize(dataContainer1);
        //    MapMemory(&data1);
        //    memcpy(data1, dataContainer1.data(), size1);
        //    auto data2 = static_cast<T1::value_type*>(data1);
        //    size_t padding = PaddingSize(size1, alignment) / sizeof(T1::value_type);
        //    std::advance(data2, dataContainer1.size() + padding);
        //    //LogWarning("Advanced size (i.e. offset): {}\n", (void*)data2 - data1);
        //    LogWarning("Addresses: data1 {}, data2 {}\n", data1, (void*)data2);
        //    LogWarning("Padding: {}, size1: {}\n", padding, size1);
        //    memcpy(data2, dataContainer2.data(), memsize(dataContainer2));
        //    UnmapMemory();
        //}

        DeviceAddress GetDeviceAddress() const {
            VkBufferDeviceAddressInfo info{
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .pNext = end_of_chain,
                .buffer = m_buffer,
            };
            return vkGetBufferDeviceAddress(m_allocator.device(), &info);
        }
    };


    
    class StbImage;
    class StagingBuffer {
        BufferVma m_buffer;
        DeviceSize m_size;
        DeviceSize m_capacity;
        Queue m_transferQueue;
    public:
        StagingBuffer(ResourceAllocator& allocator, Queue transferQueue)
            : m_buffer(allocator), m_size{ 0 }, m_capacity{ 0 }, m_transferQueue{ transferQueue } { }

        operator VkBuffer() const { return m_buffer; }

        // TODO: try std::vector -like reallocation strategy
        void LoadTexture(in_t<StbImage> stbImage);

        template<DataContainer T, DataContainer ...Ts>
        void LoadData(T const& dataContainer1, Ts const& ...dataContainers) {
            m_size = memsize(dataContainer1, dataContainers...);
            if (m_capacity < m_size) {
                ReallocateBuffer(m_size);
            }
            m_buffer.LoadData(dataContainer1, dataContainers...);
        }

        //template<DataContainer T1, DataContainer T2>
        //void LoadDataWithAlignment(DeviceSize alignment, T1& dataContainer1, T2& dataContainer2) {
        //    auto size1 = memsize(dataContainer1);
        //    auto paddingSize = PaddingSize(size1, alignment);
        //    m_size = size1 + paddingSize + memsize(dataContainer2);
        //    if (m_capacity < m_size) {
        //        ReallocateBuffer(m_size);
        //    }
        //    m_buffer.LoadDataWithAlignment(alignment, dataContainer1, dataContainer2);
        //}

        auto size            () const { return m_size;                     }
        auto capacity        () const { return m_capacity;                 }
        auto queueFamilyIndex() const { return m_transferQueue.familyIndex;}

        void WaitIdle() const {
            vkQueueWaitIdle(m_transferQueue);
        }

    private:
        void ReallocateBuffer(DeviceSize newSize) {
			BufferVmaCreateInfo info{
			    .bufferCreateInfo {
                    .size  {newSize},
                    .usage {+BufferUsageFlagBits::TransferSrc},
			    	//.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			    	//.queueFamilyIndexCount = 1u,
			    	//.pQueueFamilyIndices = &m_transferQueue.familyIndex,
			    },
			    .allocationCreateInfo {
			    	.usage = +MemoryUsage::CpuOnly,
			    },
			};
            m_buffer = m_buffer.allocator().CreateBufferVma(info);
            m_capacity = newSize;
        }

    };


    //
    // ImageVma
    // 
    class ImageVma {
        VkImage m_image;
        VmaAllocation m_vmaAllocation;
        ResourceAllocator& m_allocator;
    public:
        VK_NO_COPY_NO_DEFAULT(ImageVma);
        explicit ImageVma(ResourceAllocator& allocator) :
            m_image{null_handle}, m_vmaAllocation{null_handle}, m_allocator{allocator} {}
        ImageVma(VkImage image, VmaAllocation vmaAllocation, ResourceAllocator& allocator) :
            m_image{image}, m_vmaAllocation{vmaAllocation}, m_allocator{allocator} {}

        ImageVma(ImageVma&& image_tm) noexcept :
                m_image        { std::exchange(image_tm.m_image, null_handle)        },
                m_vmaAllocation { std::exchange(image_tm.m_vmaAllocation, null_handle) },
                m_allocator     { image_tm.m_allocator                                 }
        {
        }

        ImageVma& operator=(ImageVma&& image_tm) noexcept {
            if (this != &image_tm) {
                ImageVma tmp(std::move(image_tm));
                this->swap(tmp);
            }
            return *this;
        }

        void swap(ImageVma& image_ts) noexcept {
            if (&m_allocator != &image_ts.m_allocator) {
                LogError("Vulkan error: trying to swap images created by different allocators!\n");
            }
            std::swap(m_image       , image_ts.m_image       );
            std::swap(m_vmaAllocation, image_ts.m_vmaAllocation);
        }

        VkImage* image_ptr()    { return &m_image; }
        operator VkImage() const { return m_image; }
        ~ImageVma() {
            if (m_vmaAllocation != null_handle || m_image != null_handle) {
                vmaDestroyImage(static_cast<VmaAllocator>(m_allocator), m_image, m_vmaAllocation);
                m_vmaAllocation = null_handle;
                m_image = null_handle;
            }
        }

        ResourceAllocator& allocator() { return m_allocator; }

    };
#endif

#if !__has_include("vk_mem_alloc.h")
    //
    // Buffer
    //

    class Buffer {
        Device const&  m_device;
        VkBuffer       m_buffer;
        VkDeviceMemory m_deviceMemory;
        const VkAllocationCallbacks* m_allocator;


        uint32_t findMemoryType(uint32_t typeFilter, MemoryPropertyFlags properties) {
            auto memoryProperties = m_device.MemoryProperties();
            for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
                if ((typeFilter & (1 << i)) &&
                    ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)) {
                    return i;
                }
            }
            throw std::runtime_error("Vulkan error: no suitable memory type!");
        }
    public:
        VK_NO_COPY_NO_DEFAULT(Buffer);

        Buffer(Device const& device, const BufferCreateInfo& createInfo, MemoryPropertyFlags memoryProperties, const VkAllocationCallbacks* allocator = default_allocator) :
            m_device{ device }, m_buffer{ null_handle }, m_deviceMemory{ null_handle }, m_allocator{ allocator }
        {
            vkCreateBuffer(m_device, reinterpret_cast<const VkBufferCreateInfo*>(&createInfo), m_allocator, &m_buffer) | VK_NO_ERROR;
            // allocate and bind memory
            VkMemoryRequirements memoryRequirements;
            vkGetBufferMemoryRequirements(m_device, m_buffer, OUT &memoryRequirements);

            LogWarning("Using default memory allocation\n");
            VkMemoryAllocateInfo allocateInfo{
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = memoryRequirements.size,
                .memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
            };
            vkAllocateMemory(m_device, &allocateInfo, m_allocator, &m_deviceMemory) | VK_NO_ERROR;
            vkBindBufferMemory(m_device, m_buffer, m_deviceMemory, no_offset);
        }
        Buffer(Device const& device, VkBuffer buffer, VkDeviceMemory deviceMemory, const VkAllocationCallbacks* allocator = default_allocator) :
            m_device{device}, m_buffer{buffer}, m_deviceMemory{deviceMemory}, m_allocator{allocator} {}

        Buffer(Buffer&& buffer_tm) noexcept :
                m_device{buffer_tm.m_device},
                m_buffer{std::exchange(buffer_tm.m_buffer, null_handle)},
                m_deviceMemory{std::exchange(buffer_tm.m_deviceMemory, null_handle)},
                m_allocator{buffer_tm.m_allocator} {}

        Buffer& operator=(Buffer&& buffer_tm) noexcept {
            if (this != &buffer_tm) {
                Buffer tmp(std::move(buffer_tm));
                this->swap(tmp);
            }
            return *this;
        }

        void swap(Buffer& buffer_ts) noexcept {
            if (&m_device != &buffer_ts.m_device) {
                LogError("Vulkan error: trying to swap buffers created by different device!\n");
            }
            std::swap(m_buffer      , buffer_ts.m_buffer      );
            std::swap(m_deviceMemory, buffer_ts.m_deviceMemory);
            std::swap(m_allocator   , buffer_ts.m_allocator   );
        }

        [[nodiscard]]
        VkDeviceMemory deviceMemory() const {
            return m_deviceMemory;
        }

        VkBuffer* buffer_ptr()     { return &m_buffer; }
        operator VkBuffer() const { return m_buffer; }
        ~Buffer() {
            if (m_deviceMemory != null_handle) {
                vkFreeMemory(m_device, m_deviceMemory, m_allocator);
                m_deviceMemory = null_handle;
            }
            if (m_buffer != null_handle) {
                vkDestroyBuffer(m_device, m_buffer, m_allocator);
                m_buffer = null_handle;
            }
        }

        void MapMemory(DeviceSize offset, DeviceSize size, MemoryMapFlags flags, void** data) {
            vkMapMemory(m_device, m_deviceMemory, offset, size, flags, data) | VK_NO_ERROR;
        }
        void UnmapMemory() {
            vkUnmapMemory(m_device, m_deviceMemory);
        }
    };

    class BufferArray {
    };

    //
    // Image
    //


	class Image {
		VkDevice m_device;
		VkImage	 m_image{null_handle};
		const VkAllocationCallbacks* m_allocator{default_allocator};
	public:
		VK_NO_COPY_NO_DEFAULT(Image);
        Image(VkDevice const& device, const ImageCreateInfo& createInfo, const VkAllocationCallbacks* allocator = default_allocator) :
        m_device{device}, m_image{null_handle}, m_allocator{allocator}
        {
            vkCreateImage(m_device, reinterpret_cast<const VkImageCreateInfo*>(&createInfo), m_allocator, &m_image) | VK_NO_ERROR;
        }
        Image(VkDevice const& device, VkImage image, const VkAllocationCallbacks* allocator = default_allocator) : m_device{device}, m_image{image}, m_allocator{allocator} {}

        Image(Image&& image_tm) noexcept :
                m_device(image_tm.m_device), m_image(std::exchange(image_tm.m_image, null_handle)), m_allocator(image_tm.m_allocator) {}

        Image& operator=(Image&& image_tm) noexcept {
            if (this != &image_tm) {
                Image tmp(std::move(image_tm));
                this->swap(tmp);
            }
            return *this;
        }

        void swap(Image& image_ts) noexcept {
            std::swap(m_device    , image_ts.m_device   );
            std::swap(m_image     , image_ts.m_image    );
            std::swap(m_allocator , image_ts.m_allocator);
        }

		VkImage* image_ptr()     { return &m_image; }
		operator VkImage() const { return m_image; }
		~Image() {
            if (m_image != null_handle) {
                vkDestroyImage(m_device, m_image, m_allocator);
                m_image = null_handle;
            }
		}
	};

#endif

    //
    // Framebuffer
    //

    struct FramebufferCreateInfo {
        VkStructureType          sType{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        const void*              pNext{end_of_chain};
        VkFramebufferCreateFlags flags{};
        VkRenderPass             renderPass{};
        uint32_t                 attachmentCount{};
        const VkImageView*       pAttachments{};
        uint32_t                 width{};
        uint32_t                 height{};
        uint32_t                 layers{};
    };
    VK_CHECK_SIZE(FramebufferCreateInfo);

    class Framebuffer {
        // valid state: m_framebuffer is either null_handle or something created with m_device and m_allocator
        VkDevice m_device;
        VkFramebuffer m_framebuffer;
        const VkAllocationCallbacks* m_allocator;
    public:
        VK_NO_COPY_NO_DEFAULT(Framebuffer);
        Framebuffer(VkDevice const& device, const FramebufferCreateInfo& createInfo, const VkAllocationCallbacks* allocator = default_allocator) :
            m_device(device), m_framebuffer(), m_allocator(allocator) {
            vkCreateFramebuffer(m_device, reinterpret_cast<const VkFramebufferCreateInfo*>(&createInfo), m_allocator, &m_framebuffer) | VK_NO_ERROR;
        }

        Framebuffer(VkDevice const& device, VkFramebuffer framebuffer, const VkAllocationCallbacks* allocator = default_allocator) : m_device(device), m_framebuffer(framebuffer), m_allocator(allocator) {}

        Framebuffer(Framebuffer&& framebuffer_tm) noexcept :
                m_device(framebuffer_tm.m_device), m_framebuffer(std::exchange(framebuffer_tm.m_framebuffer, null_handle)), m_allocator(framebuffer_tm.m_allocator) {}

        Framebuffer& operator=(Framebuffer&& framebuffer_tm) noexcept {
            if (this != &framebuffer_tm) {
                Framebuffer tmp(std::move(framebuffer_tm));
                this->swap(tmp);
            }
            return *this;
        }

        void swap(Framebuffer& framebuffer_ts) noexcept {
            std::swap(m_device     , framebuffer_ts.m_device     );
            std::swap(m_framebuffer, framebuffer_ts.m_framebuffer);
            std::swap(m_allocator  , framebuffer_ts.m_allocator  );
        }

        ~Framebuffer() {
            if (m_framebuffer != null_handle) {
                vkDestroyFramebuffer(m_device, m_framebuffer, m_allocator);
                m_framebuffer = null_handle;
            }
        }

        operator VkFramebuffer() const { return m_framebuffer; }
    };


    //
    // ImageView
    //

	struct ImageViewCreateInfo {
		VkStructureType         sType			 {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
		const void*             pNext			 {end_of_chain};
		VkImageViewCreateFlags  flags			 {};
		VkImage                 image			 {};
		VkImageViewType         viewType		 {VK_IMAGE_VIEW_TYPE_2D};
		VkFormat                format			 {VK_FORMAT_UNDEFINED};
		VkComponentMapping      components		 {};
		VkImageSubresourceRange subresourceRange {};
	};
	VK_CHECK_SIZE(ImageViewCreateInfo);

    class Swapchain;
	class ImageView {
		// valid state: m_imageView is either null_handle or something created with m_device and m_allocator
		VkDevice m_device;
		VkImageView m_imageView;
		const VkAllocationCallbacks* m_allocator;
	public:
		VK_NO_COPY_NO_DEFAULT(ImageView);
		ImageView(Device const& device, const ImageViewCreateInfo& createInfo, const VkAllocationCallbacks* allocator = default_allocator) :
		    m_device(device), m_imageView(), m_allocator(allocator) {
			vkCreateImageView(m_device, reinterpret_cast<const VkImageViewCreateInfo*>(&createInfo), m_allocator, &m_imageView) | VK_NO_ERROR;
		}

		ImageView(Device const& device, VkImageView imageView, const VkAllocationCallbacks* allocator = default_allocator) : m_device(device), m_imageView(imageView), m_allocator(allocator) {}

		ImageView(ImageView&& view_tm) noexcept :
			m_device(view_tm.m_device), m_imageView(std::exchange(view_tm.m_imageView, null_handle)), m_allocator(view_tm.m_allocator) {}

		ImageView& operator=(ImageView&& view_tm) noexcept {
			if (this != &view_tm) {
				ImageView tmp(std::move(view_tm));
				this->swap(tmp);
			}
			return *this;
		}

		void swap(ImageView& view_ts) noexcept {
			std::swap(m_device   , view_ts.m_device   );
			std::swap(m_imageView, view_ts.m_imageView);
			std::swap(m_allocator, view_ts.m_allocator);
		}

		~ImageView() {
			if (m_imageView != null_handle) {
				vkDestroyImageView(m_device, m_imageView, m_allocator);
                m_imageView = null_handle;
			}
		}

		// read only
        [[nodiscard]]
		auto data() const {
			return &m_imageView;
		}

        Framebuffer createFramebuffer(const Swapchain& swapchain, VkRenderPass renderPass) const;

        operator VkImageView() const { return m_imageView; }
	};

	// 
	// Swapchain
	//
	// input:
	//		Device*
	//		Surface*
	// output:
	//		swapchain images
	//

    struct AcquireNextImageInfoKHR {
        VkStructureType    sType      {VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR};
        const void*        pNext      {end_of_chain};
        VkSwapchainKHR     swapchain  {};
        uint64_t           timeout    {disable_timeout};
        VkSemaphore        semaphore  {};
        VkFence            fence      {no_fence};
        uint32_t           deviceMask {1u};
    };
    VK_CHECK_SIZE(AcquireNextImageInfoKHR);

	class Swapchain {
		Device&				 m_device;
		Surface&			 m_surface;
		VkSwapchainKHR		 m_swapchain;
		std::vector<VkImage> swapchainImages;
		VkFormat			 swapchainImageFormat;
		VkExtent2D			 swapchainExtent;
        const VkAllocationCallbacks* m_allocator;

	public:
		VK_NO_COPY_NO_DEFAULT(Swapchain);
		operator VkSwapchainKHR() const { return m_swapchain; }
	// input:
	//		Device*
	//		Surface*
		Swapchain(Device& pDevice_, Surface& pSurface_, bool raytracing = false);

        Swapchain(Swapchain&& swapchain_tm) noexcept :
        m_device             {swapchain_tm.m_device},
        m_surface            {swapchain_tm.m_surface},
        m_swapchain          {std::exchange(swapchain_tm.m_swapchain, null_handle)},
        swapchainImages      {swapchain_tm.swapchainImages},
        swapchainImageFormat {swapchain_tm.swapchainImageFormat},
        swapchainExtent      {swapchain_tm.swapchainExtent},
        m_allocator          {swapchain_tm.m_allocator}
        {}

        Swapchain& operator=(Swapchain&& swapchain_tm) noexcept {
            if (this != &swapchain_tm) {
                Swapchain tmp(std::move(swapchain_tm));
                this->swap(tmp);
            }
            return *this;
        }

        void swap(Swapchain& swapchain_ts) noexcept {
            if (&m_device  != &swapchain_ts.m_device ||
                &m_surface != &swapchain_ts.m_surface) {
                LogError("Vulkan error: trying to swap non-matched swapchains!\n");
            }
            std::swap(m_swapchain          , swapchain_ts.m_swapchain         );
            std::swap(swapchainImages      , swapchain_ts.swapchainImages     );
            std::swap(swapchainImageFormat , swapchain_ts.swapchainImageFormat);
            std::swap(swapchainExtent      , swapchain_ts.swapchainExtent     );
            std::swap(m_allocator          , swapchain_ts.m_allocator         );
        }
		
		~Swapchain() {
            if (m_swapchain != null_handle) {
                vkDestroySwapchainKHR(m_device, m_swapchain, default_allocator);
                m_swapchain = null_handle;
            }
		}

        [[nodiscard]]
        const Device& device() const {
            return m_device;
        }

        [[nodiscard]]
        auto allocator() const {
            return m_allocator;
        }

		// output:
        [[nodiscard]]
		const auto& images() const { return swapchainImages; }

        [[nodiscard]]
        auto image(size_t index) const -> VkImage { return swapchainImages[index]; }

		std::vector<ImageView> getImageViews() ;

        [[nodiscard]]
		VkFormat imageFormat() const { return swapchainImageFormat; }
        [[nodiscard]]
		VkExtent2D extent() const { return swapchainExtent; }

        VkResult AcquireNextImage2KHR(AcquireNextImageInfoKHR& acquireInfo, uint32_t& imageIndex) {
            acquireInfo.swapchain = m_swapchain;
            return vkAcquireNextImage2KHR(m_device, reinterpret_cast<const VkAcquireNextImageInfoKHR*>(&acquireInfo), &imageIndex);
        }
	};

    //
    // PipelineLayout
    //

    struct PipelineLayoutCreateInfo {
        VkStructureType                 sType                  {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        const void*                     pNext                  {end_of_chain};
        VkPipelineLayoutCreateFlags     flags                  {};
        uint32_t                        setLayoutCount         {};
        const VkDescriptorSetLayout*    pSetLayouts            {};
        uint32_t                        pushConstantRangeCount {};
        const VkPushConstantRange*      pPushConstantRanges    {};
    };
    VK_CHECK_SIZE(PipelineLayoutCreateInfo);
    class PipelineLayout {
        // valid state: m_pipelineLayout is either null_handle or something created with m_device and m_allocator
        VkDevice m_device;
        VkPipelineLayout m_pipelineLayout;
        const VkAllocationCallbacks* m_allocator;
    public:
        VK_NO_COPY_NO_DEFAULT(PipelineLayout);
        PipelineLayout(in_t<Device> device, in_t<PipelineLayoutCreateInfo> createInfo, const VkAllocationCallbacks* allocator = default_allocator) :
            m_device(device),
            m_pipelineLayout(),
            m_allocator(allocator)
        {
            vkCreatePipelineLayout(m_device, reinterpret_cast<const VkPipelineLayoutCreateInfo*>(&createInfo), m_allocator, &m_pipelineLayout) | VK_NO_ERROR;
        }

        PipelineLayout(Device const& device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* allocator = default_allocator) :
            m_device(device), m_pipelineLayout(pipelineLayout), m_allocator(allocator) {}

        PipelineLayout(PipelineLayout&& pipelineLayout_tm) noexcept :
                m_device(pipelineLayout_tm.m_device), m_pipelineLayout(std::exchange(pipelineLayout_tm.m_pipelineLayout, null_handle)), m_allocator(pipelineLayout_tm.m_allocator) {}

        PipelineLayout& operator=(PipelineLayout&& pipelineLayout_tm) noexcept {
            if (this != &pipelineLayout_tm) {
                PipelineLayout tmp(std::move(pipelineLayout_tm));
                this->swap(tmp);
            }
            return *this;
        }

        void swap(PipelineLayout& pipelineLayout_ts) noexcept {
            std::swap(m_device        , pipelineLayout_ts.m_device        );
            std::swap(m_pipelineLayout, pipelineLayout_ts.m_pipelineLayout);
            std::swap(m_allocator     , pipelineLayout_ts.m_allocator     );
        }

        ~PipelineLayout() {
            if (m_pipelineLayout != null_handle) {
                vkDestroyPipelineLayout(m_device, m_pipelineLayout, m_allocator);
                m_pipelineLayout = null_handle;
            }
        }

        operator VkPipelineLayout() const { return m_pipelineLayout; }
    };


    //
    // RenderPass
    //

    // destroyed after render pass creation
    struct RenderPassCreateInfo {
        std::vector<VkAttachmentDescription> attachments ; // = {colorAttachment};
        std::vector<VkSubpassDescription   > subpasses   ; // = {subpass};
        std::vector<VkSubpassDependency    > dependencies; // = {dependency};
    };
    class RenderPass {
        VkDevice     m_device;
        VkRenderPass m_renderPass;
        std::vector<VkAttachmentDescription> m_attachments ; // = {colorAttachment};
        std::vector<VkSubpassDescription   > m_subpasses   ; // = {subpass};
        std::vector<VkSubpassDependency    > m_dependencies; // = {dependency};
        const VkAllocationCallbacks* m_allocator;
    public:
        VK_NO_COPY_NO_DEFAULT(RenderPass);
        RenderPass(VkDevice const& device, RenderPassCreateInfo& info, const VkAllocationCallbacks* allocator = default_allocator) :
            m_device(device),
            m_renderPass(),
            m_attachments(std::move(info.attachments)),
            m_subpasses(std::move(info.subpasses)),
            m_dependencies(std::move(info.dependencies)),
            m_allocator(allocator) {
            VkRenderPassCreateInfo renderPassInfo{
                .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                .attachmentCount = static_cast<uint32_t>(m_attachments.size()),
                .pAttachments    = m_attachments.data(),
                .subpassCount    = static_cast<uint32_t>(m_subpasses.size()),
                .pSubpasses      = m_subpasses.data(),
                .dependencyCount = static_cast<uint32_t>(m_dependencies.size()),
                .pDependencies   = m_dependencies.data(),
            };
            vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_renderPass) | VK_NO_ERROR;
        }

        RenderPass(Device const& device, VkRenderPass renderPass, const VkAllocationCallbacks* allocator = default_allocator) :
                m_device(device), m_renderPass(renderPass), m_allocator(allocator) {}

        RenderPass(RenderPass&& renderPass_tm) noexcept :
                m_device(renderPass_tm.m_device), m_renderPass(std::exchange(renderPass_tm.m_renderPass, null_handle)), m_allocator(renderPass_tm.m_allocator) {}

        RenderPass& operator=(RenderPass&& renderPass_tm) noexcept {
            if (this != &renderPass_tm) {
                RenderPass tmp(std::move(renderPass_tm));
                this->swap(tmp);
            }
            return *this;
        }

        void swap(RenderPass& renderPass_ts) noexcept {
            std::swap(m_device    , renderPass_ts.m_device    );
            std::swap(m_renderPass, renderPass_ts.m_renderPass);
            std::swap(m_allocator , renderPass_ts.m_allocator );
        }

        ~RenderPass() {
            if (m_renderPass != null_handle) {
                vkDestroyRenderPass(m_device, m_renderPass, m_allocator);
                m_renderPass = null_handle;
            }
        }

        operator VkRenderPass() const { return m_renderPass; }
    };

    //
    // DescriptorSetLayout
    //
    struct DescriptorSetLayoutCreateInfo {
        VkStructureType                     sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        const void*                         pNext{end_of_chain};
        VkDescriptorSetLayoutCreateFlags    flags{};
        uint32_t                            bindingCount{};
        const VkDescriptorSetLayoutBinding* pBindings{};
        // TODO: use a template concept
        template<Array BindingArrayType>
            requires std::same_as<typename BindingArrayType::value_type, VkDescriptorSetLayoutBinding>
        DescriptorSetLayoutCreateInfo(BindingArrayType const& bindings, VkDescriptorSetLayoutCreateFlags descriptorSetLayoutCreateFlags = 0) : flags{ descriptorSetLayoutCreateFlags } {
            bindingCount = static_cast<uint32_t>(bindings.size());
            pBindings    = bindings.data();
        }
    };
    VK_CHECK_SIZE(DescriptorSetLayoutCreateInfo);
    class DescriptorSetLayout {
        VkDevice m_device;
        VkDescriptorSetLayout m_descriptorSetLayout;
		const VkAllocationCallbacks* m_allocator;
    public:
        VK_DELETE_ALL_DEFAULT(DescriptorSetLayout);
        DescriptorSetLayout(Device const& device, DescriptorSetLayoutCreateInfo const& info, const VkAllocationCallbacks* allocator = default_allocator) : m_device{ device }, m_allocator{ allocator } { 
            vkCreateDescriptorSetLayout(m_device, reinterpret_cast<const VkDescriptorSetLayoutCreateInfo*>(&info), m_allocator, OUT& m_descriptorSetLayout) | VK_NO_ERROR;
        }
        ~DescriptorSetLayout() {
            if (m_descriptorSetLayout != null_handle) {
                vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, m_allocator);
                m_descriptorSetLayout = null_handle;
            }
        }
        operator VkDescriptorSetLayout() const { return m_descriptorSetLayout; }
        auto data()       -> VkDescriptorSetLayout*       { return &m_descriptorSetLayout; }
        auto data() const -> VkDescriptorSetLayout const* { return &m_descriptorSetLayout; }
    };

    class DescriptorSetLayoutArray {
    };

    //
    // Descriptor Set Container
    //

    //
    // Descriptor Pool
    //

    struct DescriptorPoolCreateInfo {
        VkStructureType             sType         {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        const void*                 pNext         {end_of_chain};
        VkDescriptorPoolCreateFlags flags         {};
        uint32_t                    maxSets       {};
        uint32_t                    poolSizeCount {};
        const VkDescriptorPoolSize* pPoolSizes    {};
    };
    VK_CHECK_SIZE(DescriptorPoolCreateInfo);
    class DescriptorPool {
        VkDevice m_device;
        VkDescriptorPool m_descriptorPool;
		const VkAllocationCallbacks* m_allocator;
    public:
        VK_DELETE_ALL_DEFAULT(DescriptorPool);
        DescriptorPool(in_t<Device> device, in_t<DescriptorPoolCreateInfo> info, const VkAllocationCallbacks* allocator = default_allocator) : m_device{ device }, m_allocator{ allocator } { 
            vkCreateDescriptorPool(m_device, reinterpret_cast<const VkDescriptorPoolCreateInfo*>(&info), m_allocator, OUT& m_descriptorPool) | VK_NO_ERROR;
        }
        ~DescriptorPool() {
            if (m_descriptorPool != null_handle) {
                vkDestroyDescriptorPool(m_device, m_descriptorPool, m_allocator);
                m_descriptorPool = null_handle;
            }
        }
        operator VkDescriptorPool() const { return m_descriptorPool; }

        template<Array T>
            requires std::same_as<typename T::value_type, VkDescriptorSetLayout>
        std::vector<VkDescriptorSet> AllocateDescriptorSets(T& layouts) {
            VkDescriptorSetAllocateInfo allocInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = m_descriptorPool,
                .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
                .pSetLayouts = layouts.data(),
            };
            std::vector<VkDescriptorSet> descriptorSets(layouts.size());
            vkAllocateDescriptorSets(m_device, &allocInfo, descriptorSets.data()) | VK_NO_ERROR;
            return descriptorSets;
        }
    };

    // TODO: add compute pipeline and transfer pipeline
	//
	// Graphics Pipeline
	//
	//

    // TODO: holding multiple pipelines
    // TODO: add a type field to PipelineCreateInfo and let the ctr of Pipeline use this field to determine how a pipeline is created
    struct PipelineCreateInfo {
        // const PipelineCreateInfoType = some enum
        ShaderSPIRV const&        spirv;
        Swapchain   const&        swapchain;
        PipelineLayoutCreateInfo& pipelineLayoutInfo;
        RenderPassCreateInfo&     renderPassInfo;
        std::vector<VkVertexInputBindingDescription>   const& vertexInputBindingDescriptions;
        std::vector<VkVertexInputAttributeDescription> const& vertexInputAttributeDescriptions;
    };
	class GraphicsPipeline {
		VkDevice         m_device;
        //VkRenderPass     m_renderPass    {null_handle};
        PipelineLayout   m_pipelineLayout;
        RenderPass       m_renderPass;
		VkPipeline       m_pipeline      {null_handle};
		const VkAllocationCallbacks* m_allocator;
	public:
		VK_NO_COPY_NO_DEFAULT(GraphicsPipeline);
		GraphicsPipeline(Device const& device, PipelineCreateInfo& info, const VkAllocationCallbacks* allocator = default_allocator);

		//Pipeline(Device const& device, VkPipeline pipeline, const VkAllocationCallbacks* allocator = default_allocator) :
		//	m_device(device), m_pipeline(pipeline), m_allocator(allocator) {}

		GraphicsPipeline(GraphicsPipeline&& pipeline_tm) noexcept :
                m_device(pipeline_tm.m_device),
                m_pipelineLayout(std::move(pipeline_tm.m_pipelineLayout)),
                m_renderPass(std::move(pipeline_tm.m_renderPass)),
                m_pipeline(std::exchange(pipeline_tm.m_pipeline, null_handle)),
                m_allocator(pipeline_tm.m_allocator) {}

		GraphicsPipeline& operator=(GraphicsPipeline&& pipeline_tm) noexcept {
			if (this != &pipeline_tm) {
				GraphicsPipeline tmp(std::move(pipeline_tm));
				this->swap(tmp);
			}
			return *this;
		}

		void swap(GraphicsPipeline& pipeline_ts) noexcept {
			std::swap(m_device   , pipeline_ts.m_device   );
			std::swap(m_pipeline , pipeline_ts.m_pipeline );
			std::swap(m_allocator, pipeline_ts.m_allocator);
		}

        auto& getRenderPass() const {
            return m_renderPass;
        }

		~GraphicsPipeline() {
			if (m_pipeline != null_handle) {
				vkDestroyPipeline(m_device, m_pipeline, m_allocator);
                m_pipeline = null_handle;
			}
		}

        operator VkPipeline() const { return m_pipeline; }
        VkPipelineLayout getLayout() const { return m_pipelineLayout; }
	};


    //
    //
    //

    Framebuffer createFramebuffer(VkRenderPass renderPass, const ImageView& imageView, const Swapchain& swapchain) {
        FramebufferCreateInfo createInfo{
            .renderPass      = renderPass,
            .attachmentCount = 1,
            .pAttachments    = imageView.data(),
            .width           = swapchain.extent().width,
            .height          = swapchain.extent().height,
            .layers          = 1,
        };
        return Framebuffer(swapchain.device(), createInfo, swapchain.allocator());
    }


	//
	// CommandBuffer
    // freed by command pool
	//
    struct CommandBufferInheritanceInfo {
        VkStructureType                  sType                {VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
        const void*                      pNext                {end_of_chain};
        VkRenderPass                     renderPass           {null_handle};
        uint32_t                         subpass              {0};
        VkFramebuffer                    framebuffer          {null_handle};
        VkBool32                         occlusionQueryEnable {VK_FALSE};
        VkQueryControlFlags              queryFlags           {0};
        VkQueryPipelineStatisticFlags    pipelineStatistics   {0};
    };
    VK_CHECK_SIZE(CommandBufferInheritanceInfo);
    struct CommandBufferBeginInfo {
        VkStructureType                          sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        const void*                              pNext{end_of_chain};
        VkCommandBufferUsageFlags                flags{0};
        const VkCommandBufferInheritanceInfo*    pInheritanceInfo{nullptr};
    };
    VK_CHECK_SIZE(CommandBufferBeginInfo);
    struct RenderPassBeginInfo {
        VkStructureType        sType{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        const void*            pNext{end_of_chain};
        VkRenderPass           renderPass{};
        VkFramebuffer          framebuffer{};
        VkRect2D               renderArea{};
        uint32_t               clearValueCount{};
        const VkClearValue*    pClearValues{};
    };
    VK_CHECK_SIZE(RenderPassBeginInfo);
    struct CopyBufferInfo2KHR {
        VkStructureType         sType{VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2_KHR};
        const void*             pNext{end_of_chain};
        VkBuffer                srcBuffer;
        VkBuffer                dstBuffer;
        uint32_t                regionCount;
        const VkBufferCopy2KHR* pRegions;
    };
    VK_CHECK_SIZE(CopyBufferInfo2KHR);
    struct BufferCopy2KHR {
        VkStructureType sType{VK_STRUCTURE_TYPE_BUFFER_COPY_2_KHR};
        const void*     pNext{end_of_chain};
        VkDeviceSize    srcOffset;
        VkDeviceSize    dstOffset;
        VkDeviceSize    size;
    };
    VK_CHECK_SIZE(BufferCopy2KHR);

	// TODO: reference counting?
    class CommandBufferArray;
    class CommandBuffer {
		const Device&   m_device;
		VkCommandBuffer m_buffer;
	public:
		operator VkCommandBuffer() const { return m_buffer; }
		// consume a VkCommandBuffer produced by vk::CommandPool
		CommandBuffer(const Device& device, VkCommandBuffer buffer) : m_device(device), m_buffer(buffer) {}

		void BeginRecording(in_t<CommandBufferBeginInfo> beginInfo) {
			vkBeginCommandBuffer(m_buffer, reinterpret_cast<const VkCommandBufferBeginInfo*>(&beginInfo)) | VK_NO_ERROR;
		}
        void BeginRecording() {
            CommandBufferBeginInfo beginInfo;
            vkBeginCommandBuffer(m_buffer, reinterpret_cast<const VkCommandBufferBeginInfo*>(&beginInfo)) | VK_NO_ERROR;
        }
        void BeginRecording(VkCommandBufferUsageFlags flags) {
            CommandBufferBeginInfo beginInfo{
                .flags = flags,
            };
            vkBeginCommandBuffer(m_buffer, reinterpret_cast<const VkCommandBufferBeginInfo*>(&beginInfo)) | VK_NO_ERROR;
        }
		void EndRecording() {
			vkEndCommandBuffer(m_buffer) | VK_NO_ERROR;
		}

		//
		// commands a command buffer could record
		//

		void BeginRenderPass(in_t<RenderPassBeginInfo> renderPassInfo, VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE) {
            vkCmdBeginRenderPass(m_buffer, reinterpret_cast<const VkRenderPassBeginInfo*>(&renderPassInfo), contents);
		}
        template<Array T>
            requires std::same_as<typename T::value_type, VkDescriptorSet>
		void BindDescriptorSets(VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, T& descriptorSetContainer, uint32_t dynamicOffsetCount = 0, const uint32_t* pDynamicOffsets = nullptr) {
            vkCmdBindDescriptorSets(m_buffer, pipelineBindPoint, layout, firstSet, (uint32_t)descriptorSetContainer.size(), descriptorSetContainer.data(), dynamicOffsetCount, pDynamicOffsets);
		}
        void BindIndexBuffers(VkBuffer buffer, DeviceSize offset, VkIndexType indexType = VK_INDEX_TYPE_UINT32) {
            vkCmdBindIndexBuffer(m_buffer, buffer, offset, indexType);
        }
		void BindPipeline(VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) {
			vkCmdBindPipeline(m_buffer, pipelineBindPoint, pipeline);
		}
		void BindVertexBuffers(uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const DeviceSize* pOffsets) {
            vkCmdBindVertexBuffers(m_buffer, firstBinding, bindingCount, pBuffers, pOffsets);
		}
        void BlitImage2KHR(in_t<BlitImageInfo2KHR> info) {
            vkCmdBlitImage2KHR(m_buffer, reinterpret_cast<const VkBlitImageInfo2KHR*>(&info));
        }

        void BuildAccelerationStructuresKHR(uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos) {
            vkCmdBuildAccelerationStructuresKHR(m_buffer, infoCount, pInfos, ppBuildRangeInfos);
        }

        void CopyAccelerationStructureKHR(in_t<VkCopyAccelerationStructureInfoKHR> copyInfo) {
            vkCmdCopyAccelerationStructureKHR(m_buffer, &copyInfo);
        }

        template<Array T>
            requires requires (T regions) { reinterpret_cast<const VkBufferCopy2KHR*>( regions.data() ); }
        void CopyBuffer(CopyBufferInfo2KHR& copyBufferInfo, T regions) {
            copyBufferInfo.regionCount = static_cast<uint32_t>(regions.size());
            copyBufferInfo.pRegions = reinterpret_cast<const VkBufferCopy2KHR*>(regions.data());
            vkCmdCopyBuffer2KHR(m_buffer, reinterpret_cast<const VkCopyBufferInfo2KHR*>(&copyBufferInfo));
        }
        void CopyImage2KHR(in_t<CopyImageInfo2KHR> info) {
            vkCmdCopyImage2KHR(m_buffer, reinterpret_cast<const VkCopyImageInfo2KHR*>(&info));
        }
		void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
            vkCmdDraw(m_buffer, vertexCount, instanceCount, firstVertex, firstInstance);
		}
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
            vkCmdDrawIndexed(m_buffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        }
        void DrawMeshTasksNV(uint32_t taskCount, uint32_t firstTask) {
            vkCmdDrawMeshTasksNV(m_buffer, taskCount, firstTask);
        }
        void EndRenderPass() {
            vkCmdEndRenderPass(m_buffer);
        }

        void ExecuteCommandBuffers(in_t<CommandBufferArray> cmdBuffers);

		void PipelineBarrier2KHR(in_t<DependencyInfoKHR> info) {
			vkCmdPipelineBarrier2KHR(m_buffer, reinterpret_cast<const VkDependencyInfoKHR*>(&info));
		}

        template<uint32_t numMB, uint32_t numBMB, uint32_t numIMB>
        void PipelineBarrier2KHR(VkDependencyFlags              flags,
            std::array<MemoryBarrier2KHR,       numMB>  const& memoryBarriers,
            std::array<BufferMemoryBarrier2KHR, numBMB> const& bufferMemoryBarriers,
            std::array<ImageMemoryBarrier2KHR,  numIMB> const& imageMemoryBarriers)
        {
            auto info = VkDependencyInfoKHR{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
                .dependencyFlags          = flags,
                .memoryBarrierCount       = numMB,
                .pMemoryBarriers          = reinterpret_cast<const VkMemoryBarrier2KHR*>(memoryBarriers.data()),
                .bufferMemoryBarrierCount = numBMB,
                .pBufferMemoryBarriers    = reinterpret_cast<const VkBufferMemoryBarrier2KHR*>(bufferMemoryBarriers.data()),
                .imageMemoryBarrierCount  = numIMB,
                .pImageMemoryBarriers     = reinterpret_cast<const VkImageMemoryBarrier2KHR*>(imageMemoryBarriers.data()),
            };
            vkCmdPipelineBarrier2KHR(m_buffer, &info);
        }

        template<uint32_t numMB>
        void PipelineMemoryBarrier2KHR(std::array<MemoryBarrier2KHR, numMB> const& memoryBarriers, VkDependencyFlags flags = 0) {
            auto info = VkDependencyInfoKHR{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
                .dependencyFlags          = flags,
                .memoryBarrierCount       = numMB,
                .pMemoryBarriers          = reinterpret_cast<const VkMemoryBarrier2KHR*>(memoryBarriers.data()),
            };
            vkCmdPipelineBarrier2KHR(m_buffer, &info);
        }
        template<uint32_t numBMB>
        void PipelineBufferMemoryBarrier2KHR(std::array<BufferMemoryBarrier2KHR, numBMB> const& bufferMemoryBarriers, VkDependencyFlags flags = 0) {
            auto info = VkDependencyInfoKHR{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
                .dependencyFlags          = flags,
                .bufferMemoryBarrierCount = numBMB,
                .pBufferMemoryBarriers    = reinterpret_cast<const VkBufferMemoryBarrier2KHR*>(bufferMemoryBarriers.data()),
            };
            vkCmdPipelineBarrier2KHR(m_buffer, &info);
        }
        template<uint32_t numIMB>
        void PipelineImageMemoryBarrier2KHR(std::array<ImageMemoryBarrier2KHR, numIMB> const& imageMemoryBarriers, VkDependencyFlags flags = 0) {
            auto info = VkDependencyInfoKHR{
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
                .dependencyFlags          = flags,
                .imageMemoryBarrierCount  = numIMB,
                .pImageMemoryBarriers     = reinterpret_cast<const VkImageMemoryBarrier2KHR*>(imageMemoryBarriers.data()),
            };
            vkCmdPipelineBarrier2KHR(m_buffer, &info);
        }

        void PushConstants(VkPipelineLayout layout, VkPushConstantRange pushConstantRange, const void* pValues) {
            vkCmdPushConstants(m_buffer, layout, pushConstantRange.stageFlags, pushConstantRange.offset, pushConstantRange.size, pValues);
        }

        // TODO: make it type-safe
        void TraceRaysKHR(in_t<VkStridedDeviceAddressRegionKHR> raygenShaderBindingTable,
                          in_t<VkStridedDeviceAddressRegionKHR> missShaderBindingTable,
                          in_t<VkStridedDeviceAddressRegionKHR> hitShaderBindingTable,
                          in_t<VkStridedDeviceAddressRegionKHR> callableShaderBindingTable,
                          uint32_t width, uint32_t height, uint32_t depth)
        {
            vkCmdTraceRaysKHR(m_buffer, &raygenShaderBindingTable, &missShaderBindingTable, &hitShaderBindingTable, &callableShaderBindingTable, width, height, depth);
        }

        void WriteAccelerationStructurePropertiesKHR(uint32_t accelerationStructureCount, const VkAccelerationStructureKHR* pAccelerationStructures, VkQueryType queryType, VkQueryPool queryPool, uint32_t firstQuery) {
            vkCmdWriteAccelerationStructuresPropertiesKHR(m_buffer, accelerationStructureCount, pAccelerationStructures, queryType, queryPool, firstQuery);
        }
	};


    //
    // CommandBufferArray
	//      container of command buffers to be submitted to a queue as a batch
    //
    class CommandPool;
	class CommandBufferArray {
        const Device&      m_device;
        const CommandPool& m_pool;
		std::vector<VkCommandBuffer> m_commandBuffers;
	public:
        VK_NO_COPY_NO_DEFAULT(CommandBufferArray);
        CommandBufferArray(const Device& device, const CommandPool& pool) :
            m_device{device}, m_pool{pool}, m_commandBuffers{} {}
        CommandBufferArray(const Device& device, const CommandPool& pool, std::vector<VkCommandBuffer>&& commandBuffers) :
            m_device{device}, m_pool{pool}, m_commandBuffers{std::move(commandBuffers)} {}
        ~CommandBufferArray();

        CommandBufferArray(CommandBufferArray&& commandBufferArray_tm) noexcept :
            m_device{commandBufferArray_tm.m_device}, m_pool{commandBufferArray_tm.m_pool}, m_commandBuffers{std::exchange(commandBufferArray_tm.m_commandBuffers, {})} {}

        CommandBufferArray& operator=(CommandBufferArray&& commandBufferArray_tm) noexcept {
            if (this != &commandBufferArray_tm) {
                CommandBufferArray tmp(std::move(commandBufferArray_tm));
                this->swap(tmp);
            }
            return *this;
        }

        void swap(CommandBufferArray& commandBufferArray_ts) noexcept {
            if (&m_device != &commandBufferArray_ts.m_device ||
                &m_pool != &commandBufferArray_ts.m_pool) {
                LogError("Vulkan error: trying to swap incompatible command buffer array!\n");
            }
            std::swap(m_commandBuffers,    commandBufferArray_ts.m_commandBuffers);
        }

        CommandBuffer operator[](size_t index) {
            return CommandBuffer(m_device, m_commandBuffers[index]);
        }
        //CommandBuffer Cmd(size_t index) {
        //    return CommandBuffer(m_device, m_commandBuffers[index]);
        //}
		void push_back(CommandBuffer const& cmdBuffer) {
			m_commandBuffers.push_back(cmdBuffer);
		}
		void push_back(VkCommandBuffer cmdBuffer) {
			m_commandBuffers.push_back(cmdBuffer);
		}

        auto size() const {
            return m_commandBuffers.size();
        }

        auto data() {
            return m_commandBuffers.data();
        }
        auto data() const {
            return m_commandBuffers.data();
        }

        auto empty() const {
            return m_commandBuffers.empty();
        }

        auto begin() { return m_commandBuffers.begin(); }
        auto end()   { return m_commandBuffers.end(); }

        auto cbegin() const { return m_commandBuffers.cbegin(); }
        auto cend()   const { return m_commandBuffers.cend(); }

        auto rbegin() { return m_commandBuffers.rbegin(); }
        auto rend()   { return m_commandBuffers.rend(); }

		void SubmitAllToQueue(Queue queue, VkSubmitInfo& submitInfo, VkFence fence) {
            submitInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());
            submitInfo.pCommandBuffers    = m_commandBuffers.data();
            vkQueueSubmit(queue, 1, &submitInfo, fence) | VK_NO_ERROR;
		}

        void SubmitToQueue2KHR(Queue queue, VkSubmitInfo2KHR& submitInfo, VkFence fence) {
            std::vector commandBufferSubmitInfo(m_commandBuffers.size(), VkCommandBufferSubmitInfoKHR{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR});
            for (auto i : iota(0uz, m_commandBuffers.size())) {
                commandBufferSubmitInfo[i].commandBuffer = m_commandBuffers[i];
            }
            submitInfo.sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR;
            submitInfo.commandBufferInfoCount = static_cast<uint32_t>(commandBufferSubmitInfo.size());
            submitInfo.pCommandBufferInfos    = commandBufferSubmitInfo.data();
            vkQueueSubmit2KHR(queue, 1, &submitInfo, fence) | VK_NO_ERROR;
        }

        void SubmitToQueue(Queue queue, ptrdiff_t index, VkSubmitInfo& submitInfo, VkFence fence) {
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers    = &m_commandBuffers[index];
            vkQueueSubmit(queue, 1, &submitInfo, fence) | VK_NO_ERROR;
        }
        template <Array T>
            requires std::convertible_to<typename T::value_type, ptrdiff_t>
        void SubmitToQueue(Queue queue, T indices, VkSubmitInfo& submitInfo, VkFence fence) {
            std::vector<VkCommandBuffer> bufferToSubmit(indices.size());
            for (auto i : iota(0uz, indices.size())) {
                bufferToSubmit[i] = m_commandBuffers[indices[i]];
            }
            submitInfo.commandBufferCount = static_cast<uint32_t>(bufferToSubmit.size());
            submitInfo.pCommandBuffers    = bufferToSubmit.data();
            vkQueueSubmit(queue, 1, &submitInfo, fence) | VK_NO_ERROR;
        }

        // TODO: add a method to submit command buffers indexed by a vector to a queue
        // TODO: sync2 version
        // TODO: add a method to collect CommandBuffer
	};

	//
	// CommandPool
	//
    struct CommandBufferAllocateInfo {
        VkStructureType       sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        const void*           pNext{end_of_chain};
        VkCommandPool         commandPool{};
        VkCommandBufferLevel  level{};
        uint32_t              commandBufferCount{};
    };
    VK_CHECK_SIZE(CommandBufferAllocateInfo);
	struct CommandPoolCreateInfo {
        VkStructureType             sType{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        const void*                 pNext{end_of_chain};
        VkCommandPoolCreateFlags    flags{0};
        uint32_t                    queueFamilyIndex{};
	};
    VK_CHECK_SIZE(CommandPoolCreateInfo);
	class CommandPool {
        // we need to pass this pointer to CommandBuffer so that CommandBuffer know those extension functions
		const Device& m_device;
		VkCommandPool m_pool;
        const VkAllocationCallbacks* m_allocator;
	public:
		VK_DELETE_ALL_DEFAULT(CommandPool);
		operator VkCommandPool() const { return m_pool; }
		CommandPool(const Device& device, const CommandPoolCreateInfo& commandPoolCreateInfo, const VkAllocationCallbacks* allocator = default_allocator) :
		    m_device(device),
            m_pool(nullptr),
            m_allocator(allocator)
        {
			vkCreateCommandPool(m_device, reinterpret_cast<const VkCommandPoolCreateInfo*>(&commandPoolCreateInfo), m_allocator, &m_pool) | VK_NO_ERROR;
		}

        ~CommandPool() {
			if (m_pool != null_handle) {
				vkDestroyCommandPool(m_device, m_pool, m_allocator);
                m_pool = null_handle;
			}
        }

        auto device() const -> Device const& {
            return m_device;
        }

        void Reset() {
            vkResetCommandPool(m_device, m_pool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT) | VK_NO_ERROR;
        }

        [[nodiscard]]
        CommandBufferArray AllocateSingleCommandBuffer() {
            VkCommandBufferAllocateInfo allocateInfo{
                .sType              {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO},
                .commandPool        {m_pool},
                .level              {VK_COMMAND_BUFFER_LEVEL_PRIMARY},
                .commandBufferCount {1},
            };
            std::vector<VkCommandBuffer> cmdBuffers(1);
            vkAllocateCommandBuffers(m_device, &allocateInfo, cmdBuffers.data()) | VK_NO_ERROR;
            return CommandBufferArray(m_device, *this, std::move(cmdBuffers));
        }

#if 0
		std::vector<CommandBuffer> allocateCommandBuffer(CommandBufferAllocateInfo& info) {
            info.commandPool = m_pool;
			std::vector<VkCommandBuffer> buffers(info.commandBufferCount);
			vkAllocateCommandBuffers(m_device, reinterpret_cast<const VkCommandBufferAllocateInfo*>(&info), buffers.data()) | VK_NO_ERROR;
            std::vector<CommandBuffer> commandBuffers;
            for (auto buffer : buffers) {
                commandBuffers.emplace_back(m_device, buffer);
            }
            return commandBuffers;
		}
#endif

        CommandBufferArray AllocateCommandBufferArray(inout_t<CommandBufferAllocateInfo> info) {
            info.commandPool = m_pool;
			std::vector<VkCommandBuffer> buffers(info.commandBufferCount);
			vkAllocateCommandBuffers(m_device, reinterpret_cast<const VkCommandBufferAllocateInfo*>(&info), buffers.data()) | VK_NO_ERROR;
            return CommandBufferArray(m_device, *this, std::move(buffers));
        }

	};

    //
    // QueryPool
    //
    struct QueryPoolCreateInfo{
        VkStructureType               sType              {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        const void*                   pNext              {};
        VkQueryPoolCreateFlags        flags              {};
        VkQueryType                   queryType          {};
        uint32_t                      queryCount         {};
        VkQueryPipelineStatisticFlags pipelineStatistics {};
    };
    VK_CHECK_SIZE(QueryPoolCreateInfo);
    class QueryPool {
        VkDevice    m_device;
        VkQueryPool m_queryPool;
        VkAllocationCallbacks const* m_allocator;
    public:
        VK_DELETE_ALL_DEFAULT(QueryPool);
        QueryPool(in_t<Device> device, in_t<QueryPoolCreateInfo> createInfo, VkAllocationCallbacks const* allocator = default_allocator)
            : m_device(device), m_queryPool{null_handle}, m_allocator{ default_allocator }
        {
            vkCreateQueryPool(m_device, reinterpret_cast<const VkQueryPoolCreateInfo*>(&createInfo), allocator, OUT& m_queryPool) | VK_NO_ERROR;
        }
        ~QueryPool() {
            if (m_queryPool != null_handle) {
                vkDestroyQueryPool(m_device, m_queryPool, m_allocator);
                m_queryPool = null_handle;
            }
        }
        operator VkQueryPool() const { return m_queryPool; }

        auto GetQueryResults(uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags) const -> void {
            vkGetQueryPoolResults(m_device, m_queryPool, firstQuery, queryCount, dataSize, pData, stride, flags) | VK_NO_ERROR;
        }

        auto vkResetQueryPoolEXT(uint32_t firstQuery, uint32_t queryCount) -> void {
            vkResetQueryPool(m_device, m_queryPool, firstQuery, queryCount);
        }
    };


    // TODO: Use Synchronization2 extension
    //
    // Semaphore
    //
    // input:
    // output:
    //		RAII class Semaphore
    //

    class Semaphore {
        // valid state: m_semaphore is either null_handle or something created with m_device and m_allocator
        VkDevice m_device;
        VkSemaphore m_semaphore;
        const VkAllocationCallbacks* m_allocator;
    public:
        VK_NO_COPY_NO_DEFAULT(Semaphore);
        Semaphore(Device const& device, const VkAllocationCallbacks* allocator) :
            m_device(device), m_semaphore(nullptr), m_allocator(allocator) {
            VkSemaphoreCreateInfo createInfo{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            };
            vkCreateSemaphore(m_device, &createInfo, m_allocator, &m_semaphore) | VK_NO_ERROR;
        }

        Semaphore(Device const& device, VkSemaphore semaphore, const VkAllocationCallbacks* allocator) :
                m_device(device), m_semaphore(semaphore), m_allocator(allocator) {}

        Semaphore(Semaphore&& semaphore_tm) noexcept :
                m_device(semaphore_tm.m_device), m_semaphore(std::exchange(semaphore_tm.m_semaphore, null_handle)), m_allocator(semaphore_tm.m_allocator) {}

        Semaphore& operator=(Semaphore&& semaphore_tm) noexcept {
            if (this != &semaphore_tm) {
                Semaphore tmp(std::move(semaphore_tm));
                this->swap(tmp);
            }
            return *this;
        }

        void swap(Semaphore& semaphore_ts) noexcept {
            std::swap(m_device   , semaphore_ts.m_device   );
            std::swap(m_semaphore, semaphore_ts.m_semaphore);
            std::swap(m_allocator, semaphore_ts.m_allocator);
        }

        ~Semaphore() {
            if (m_semaphore != null_handle) {
                vkDeviceWaitIdle(m_device);
                vkDestroySemaphore(m_device, m_semaphore, m_allocator);
                m_semaphore = null_handle;
            }
        }

        operator VkSemaphore() const { return m_semaphore; }
    };

    // TODO: Use Synchronization2 extension
    //
    // Fence
    //
    // input:
    // output:
    //		RAII class Fence
    //

    class Fence {
        // valid state: m_fence is either null_handle or something created with m_device and m_allocator
        VkDevice m_device;
        VkFence m_fence;
        const VkAllocationCallbacks* m_allocator;
    public:
        VK_NO_COPY_NO_DEFAULT(Fence);
        explicit Fence(Device const& device, FenceCreateFlagBits initialSignal = FenceCreateFlagBits::Unsignaled, const VkAllocationCallbacks* allocator = default_allocator) :
                m_device(device), m_fence(nullptr), m_allocator(allocator) {
            VkFenceCreateInfo createInfo{
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = +initialSignal,
            };
            vkCreateFence(m_device, &createInfo, m_allocator, &m_fence) | VK_NO_ERROR;
        }

        Fence(Device const& device, VkFence fence, const VkAllocationCallbacks* allocator) :
                m_device(device), m_fence(fence), m_allocator(allocator) {}

        Fence(Fence&& fence_tm) noexcept :
                m_device(fence_tm.m_device), m_fence(std::exchange(fence_tm.m_fence, null_handle)), m_allocator(fence_tm.m_allocator) {}

        Fence& operator=(Fence&& fence_tm) noexcept {
            if (this != &fence_tm) {
                Fence tmp(std::move(fence_tm));
                this->swap(tmp);
            }
            return *this;
        }

        void swap(Fence& fence_ts) noexcept {
            std::swap(m_device   , fence_ts.m_device   );
            std::swap(m_fence, fence_ts.m_fence);
            std::swap(m_allocator, fence_ts.m_allocator);
        }

        ~Fence() {
            if (m_fence != null_handle) {
                vkDeviceWaitIdle(m_device);
                vkDestroyFence(m_device, m_fence, m_allocator);
                m_fence = null_handle;
            }
        }

        operator VkFence() const { return m_fence; }

        // no timeout
        void Wait() const {
            vkWaitForFences(m_device, 1, &m_fence, wait_all, disable_timeout);
        }

        void Reset() {
            vkResetFences(m_device, 1, &m_fence);
        }
    };

//
// Resources (image, obj, ...
//

    //
    // stb_image
    //
    class StbImage {
        std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels;
        int m_imageWidth;
        int m_imageHeight;
        int m_imageChannels;
        DeviceSize m_imageSize;

    public:
        StbImage(in_t<fs::path> filename, int imageFormat = STBI_rgb_alpha) :
            pixels(stbi_load(filename.string().c_str(), &m_imageWidth, &m_imageHeight, &m_imageChannels, imageFormat), &stbi_image_free)
        {
            m_imageSize = (DeviceSize)m_imageWidth * m_imageHeight * imageFormat;
            if (not pixels) {
                throw std::runtime_error(fmt::format("Stb_image error: failed to load image file {}!", filename.string()));
            }
        }
        auto data() -> void* {
            return pixels.get();
        }
        auto data() const -> const void* {
            return pixels.get();
        }
        auto size() const -> DeviceSize {
            return m_imageSize;
        }
    };

    //
    // TinyObjLoader
    // (experimental) optimized version
    //
    // TODO: move to a separate module partition
    //


    class ObjModel {
        bool m_empty{ true };
        tinyobj_opt::attrib_t m_attrib;
        std::vector<tinyobj_opt::shape_t> m_shapes;
        std::vector<tinyobj_opt::material_t> m_materials;
        tinyobj_opt::LoadOption m_loadOption;
        std::vector<uint32_t> m_indices;
    public:
        using VertexComponentType = float;
        static_assert(std::same_as<decltype(m_attrib.vertices)::value_type, VertexComponentType>);
        using TriangleVertexType = float[3];
        using QuadVertexType = float[4];
        using IndexType = tinyobj_opt::index_t;
        using VertexLocationType = uint32_t;
        static_assert(std::same_as<decltype(m_indices)::value_type, VertexLocationType>);
        using IndexLocationType = uint32_t;
        using IndexComponentType = VertexLocationType;
        using ShapeType = tinyobj_opt::shape_t;
        using MaterialType = tinyobj_opt::material_t;
            
        void LoadObj(const fs::path& objFilePath) {
            Clear();
            LogInfo("Loading .obj file: {}\n", objFilePath.string());
            std::string m_objFile = LoadFile(objFilePath, false);
            auto result = tinyobj_opt::parseObj(&m_attrib, &m_shapes, &m_materials, m_objFile.c_str(), m_objFile.length(), m_loadOption);
            if (!result) {
                throw std::runtime_error("Obj loader error: failed to load obj model(s)!");
            }
            LoadIndices();
            m_empty = false;
        }
        void SetMaterials() {
            LogWarning("Material size: {}\n", m_materials.size());
            MaterialSrgbToLinear();
        }
        void MaterialSrgbToLinear() {
            // gamma correction
        }
        // TODO: multithreaded loading
        void LoadIndices() {
            m_indices.resize(m_attrib.indices.size());
            for (auto i : iota(0uz, m_indices.size())) {
                m_indices[i] = static_cast<uint32_t>(m_attrib.indices[i].vertex_index);
            }
        }
        //void LoadBuffer() {
        //}
        void Clear() {
            if (m_empty) return;
            m_attrib = tinyobj_opt::attrib_t();
            m_shapes.clear();
            m_materials.clear();
            m_loadOption = tinyobj_opt::LoadOption();
            m_empty = true;
        }
        const auto& vertices() const {
            return m_attrib.vertices;
        }
        const auto& indices() const {
            return m_indices;
        }
        const auto& normals() const {
            return m_attrib.normals;
        }
    };

    //
    // Vertex buffer helpers
    //


	struct Vertex {
		glm::vec2 pos;
		glm::vec3 color;

		static VkVertexInputBindingDescription2EXT BindingDescription2EXT() {
			return {
					.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT,
					.binding = 0,
					.stride = sizeof(Vertex),
					.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
					.divisor = 1,
			};
		}

		static std::vector<VkVertexInputBindingDescription> BindingDescriptions() {
			return {
					VkVertexInputBindingDescription {
							.binding = 0,
							.stride = sizeof(Vertex),
							.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
					}
			};
		}

		static std::vector<VkVertexInputAttributeDescription> AttributeDescriptions() {
			return {
					VkVertexInputAttributeDescription {
						.location = 0,
						.binding = 0,
						.format = VK_FORMAT_R32G32_SFLOAT,
						.offset = static_cast<uint32_t>(offsetof(Vertex, pos)),
					},
					{
						.location = 1,
						.binding = 0,
						.format = VK_FORMAT_R32G32B32_SFLOAT,
						.offset = static_cast<uint32_t>(offsetof(Vertex, color)),
					}
			};
		}
	};

	struct VertexData {
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
	};

    auto CreateVertexBuffer(DeviceSize bufferSize, ResourceAllocator& allocator, std::initializer_list<QueueFamilyIndexType> queueFamilies, bool raytracing = false) {
        bool separateTransferQ = *std::begin(queueFamilies) != *std::rbegin(queueFamilies);
        BufferVmaCreateInfo vertexBufferVmaCreateInfo{
            .bufferCreateInfo {
                .size = bufferSize,
                .usage = BufferUsageFlagBits::TransferDst | BufferUsageFlagBits::StorageBuffer | BufferUsageFlagBits::VertexBuffer | BufferUsageFlagBits::IndexBuffer,
                .sharingMode = separateTransferQ ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = separateTransferQ ? 2u : 1u,
                .pQueueFamilyIndices = queueFamilies.begin(),
            },
            .allocationCreateInfo {
                .usage = +MemoryUsage::GpuOnly,
            },
        };
        if (raytracing) {
            vertexBufferVmaCreateInfo.bufferCreateInfo.usage |= BufferUsageFlagBits::ShaderDeviceAddress | BufferUsageFlagBits::AccelerationStructureBuildInputReadOnlyKhr;
        }
        return allocator.CreateBufferVma(vertexBufferVmaCreateInfo);
    }
    auto CreateVertexBuffer(const VertexData& vertexData, ResourceAllocator& allocator, std::initializer_list<QueueFamilyIndexType> queueFamilies, bool raytracing = false) {
        return CreateVertexBuffer(memsize(vertexData.vertices) + memsize(vertexData.indices), allocator, queueFamilies, raytracing);
    }

    auto RunSingleCommand(inout_t<CommandPool> cmdPool, in_t<Queue> queue, inout_t<std::function<void(CommandBuffer&)>> RecordCommands) {
        CommandBufferAllocateInfo cmdBufferAllocateInfo {
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        auto commandBuffers = cmdPool.AllocateCommandBufferArray(cmdBufferAllocateInfo);
        auto cmdBuf = commandBuffers[0];
        cmdBuf.BeginRecording(+CommandBufferUsageFlagBits::OneTimeSubmit);
        RecordCommands(cmdBuf);
        cmdBuf.EndRecording();
    
        VkSubmitInfo2KHR submitInfo{};
        Fence fence(cmdPool.device());
        commandBuffers.SubmitToQueue2KHR(queue, submitInfo, fence);
        fence.Wait();
    }

    // TODO: implement a mesh shader version
    struct MeshUploadingContext {
        Fence fence;
        Queue& queue;
        CommandPool& pool;
        CommandBufferArray commandBuffers;
        BufferVma& vertexBuffer;
        StagingBuffer& stagingBuffer;
        VertexData const& vertexData;
        DeviceSize bufferSize;
    };
    void UploadMesh(MeshUploadingContext& context) {
        context.stagingBuffer.LoadData(context.vertexData.vertices, context.vertexData.indices);
    
        CommandBufferAllocateInfo cmdBufferAllocateInfo {
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        context.commandBuffers = context.pool.AllocateCommandBufferArray(cmdBufferAllocateInfo);
        auto cmdBuf = context.commandBuffers[0];
        cmdBuf.BeginRecording(+CommandBufferUsageFlagBits::OneTimeSubmit);
        {
            std::vector regions = { BufferCopy2KHR{
                .srcOffset = 0,
                .dstOffset = 0,
                .size = context.bufferSize,
            } };
            CopyBufferInfo2KHR copyBufferInfo{
                .srcBuffer = context.stagingBuffer,
                .dstBuffer = context.vertexBuffer,
            };
            cmdBuf.CopyBuffer(copyBufferInfo, regions);
        }
        cmdBuf.EndRecording();
    
        VkSubmitInfo2KHR submitInfo{};
        context.commandBuffers.SubmitToQueue2KHR(context.queue, submitInfo, context.fence);
        //context.fence.Wait();
    }

    //
    // #Raytracing
    //
    // Ray Tracing Context:
    //
    // ---- load obj models
    // ---- build acceleration structure (both BLAS and TLAS)
    // 
    // code style is slightly different from the other part; try to follow Always Auto
    //


    // glm to VkTransformMatrixKHR
    auto TransformMatrix(glm::mat4 transform) -> VkTransformMatrixKHR {
        transform /= transform[3][3];
        return {
            transform[0][0], transform[0][1], transform[0][2], transform[0][3],
            transform[1][0], transform[1][1], transform[1][2], transform[1][3],
            transform[2][0], transform[2][1], transform[2][2], transform[2][3],
        };
    }
    auto TransformMatrix(glm::mat4 rotate, glm::vec3 translate) -> VkTransformMatrixKHR {
        return {
            rotate[0][0], rotate[0][1], rotate[0][2], translate[0],
            rotate[1][0], rotate[1][1], rotate[1][2], translate[1],
            rotate[2][0], rotate[2][1], rotate[2][2], translate[2],
        };
    }
    auto TransformMatrix(glm::mat3 rotate, glm::vec3 translate) -> VkTransformMatrixKHR {
        return {
            rotate[0][0], rotate[0][1], rotate[0][2], translate[0],
            rotate[1][0], rotate[1][1], rotate[1][2], translate[1],
            rotate[2][0], rotate[2][1], rotate[2][2], translate[2],
        };
    }

//#define VK_RAY_TRACING_VERBOSE
    struct RayTracingContextCreateInfo {
        struct InstanceInfo {
            std::vector<glm::mat3> rotates;
            std::vector<glm::vec3> translates;
            auto count() const noexcept {
                return rotates.size();
            }
        };
        struct ObjectProperty {
            fs::path pathToModel;
            InstanceInfo instanceInfo;
            // uint32_t sbtOffset;
        };
        Device const& device;
        //std::vector<fs::path> pathsToModels;
        //std::vector<InstanceInfo> instanceInfos;
        std::vector<ObjectProperty> sceneInfo;
        StagingBuffer& stagingBuffer;
        ResourceAllocator& allocator;
        CommandPool& transferCmdPool;
        CommandPool& raytracingCmdPool;
    };
    class RayTracingContext {
        Device const& m_device;
        Queue transferQueue;
        Queue raytracingQueue;
        ResourceAllocator& m_allocator;
        CommandPool& transferCmdPool;
        CommandPool& raytracingCmdPool;

        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
        };
    public:
        auto GetShaderGroupHandleSize() const -> uint32_t {
            return rayTracingProperties.shaderGroupHandleSize;
        }
        auto GetShaderGroupBaseAlignment() const -> uint32_t {
            return rayTracingProperties.shaderGroupBaseAlignment;
        }
        auto GetShaderGroupHandleAlignment() const -> uint32_t {
            return rayTracingProperties.shaderGroupHandleAlignment;
        }
        auto GetMaxShaderGroupStride() const -> uint32_t {
            return rayTracingProperties.maxShaderGroupStride;
        }

    // loading vertices
    private:
        std::vector<ObjModel>    m_objs;
        std::vector<std::string> m_filenames;
        std::vector<BufferVma>   m_vertexBuffers;
        std::vector<ObjModel::VertexLocationType> m_vertexLocations;
        std::vector<ObjModel::IndexLocationType>  m_indexLocations;
        DeviceSize               vertexBufferSize{};
        DeviceSize               vertexOffset{};
        DeviceSize               indexBufferSize{};
        DeviceSize               indexOffset{};
        DeviceSize               vertexLocationBufferSize{};
        DeviceSize               vertexLocationOffset{};
        DeviceSize               indexLocationBufferSize{};
        DeviceSize               indexLocationOffset{};
        //DeviceSize               normalBufferSize;
        //DeviceSize               normalOffset;

        //auto LoadObjs(in_t<std::vector<fs::path>> pathsToModels) -> void {
        auto LoadObjs(in_t<std::vector<RayTracingContextCreateInfo::ObjectProperty>> sceneInfo) -> void {
            auto objNum = sceneInfo.size(); // number of BLASs
            m_objs.resize(objNum);
            m_filenames.resize(objNum);
            for (auto i : iota(0uz, objNum)) {
                const auto& pathToModel = sceneInfo[i].pathToModel;
                m_objs[i].LoadObj(pathToModel);
                m_objs[i].SetMaterials(); // not implemented yet
                m_filenames[i] = pathToModel.filename().string();
            }

        }
        auto InitRayTracing() -> void {
            VkPhysicalDeviceProperties2 properties2 {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                .pNext = &rayTracingProperties,
            };
            vkGetPhysicalDeviceProperties2(m_device, &properties2);
#ifdef VK_RAY_TRACING_INIT_VERBOSE
#define VK_RAY_TRACING_PRINT(var) LogInfo(#var ": {}\n", rayTracingProperties.var)
               VK_RAY_TRACING_PRINT(shaderGroupHandleSize);
               VK_RAY_TRACING_PRINT(maxRayRecursionDepth);
               VK_RAY_TRACING_PRINT(maxShaderGroupStride);
               VK_RAY_TRACING_PRINT(shaderGroupBaseAlignment);
               VK_RAY_TRACING_PRINT(shaderGroupHandleCaptureReplaySize);
               VK_RAY_TRACING_PRINT(maxRayDispatchInvocationCount);
               VK_RAY_TRACING_PRINT(shaderGroupHandleAlignment);
               VK_RAY_TRACING_PRINT(maxRayHitAttributeSize);
#undef RT_PRINT
#endif
        }

		auto LoadObjsToBuffers(
            out_t<StagingBuffer> stagingBuffer)
            -> void
        {
            auto objNum = m_objs.size();

            auto max_vertices = 0uz;
            auto max_indices = 0uz;
            for (auto obj_index : iota(0uz, objNum)) {
                const auto& obj = m_objs[obj_index];
                max_vertices += obj.vertices().size();
                max_indices += obj.indices().size();
            }
            std::vector<float> vertices;
            std::vector<ObjModel::VertexLocationType> indices;
            vertices.reserve(max_vertices);
            indices.reserve(max_indices);
            m_vertexLocations.reserve(objNum);
            m_indexLocations.reserve (objNum);

            vertexBufferSize = 0;
            indexBufferSize = 0;
            for (auto obj_index : iota(0uz, objNum)) {
                const auto& obj = m_objs[obj_index];

                m_vertexLocations.push_back(vertices.size());

                vertices.insert(vertices.end(), obj.vertices().begin(), obj.vertices().end());
                vertexBufferSize += memsize(obj.vertices());

                m_indexLocations.push_back(indices.size());

                indices.insert(indices.end(), obj.indices().begin(), obj.indices().end());
                indexBufferSize += memsize(obj.indices());
            }

            auto alignment   = m_device.getProperties().limits.minStorageBufferOffsetAlignment;
            auto bufferEndAddress = DeviceAddress(0);

            vertexOffset     = bufferEndAddress;
            bufferEndAddress = vertexOffset + vertexBufferSize;

            //auto indexPaddingSize = PaddingSize(vertexBufferSize, alignment);
            auto indexPaddingSize = PaddingSize(bufferEndAddress, alignment);
            indexOffset      = bufferEndAddress + indexPaddingSize;
            bufferEndAddress = indexOffset + indexBufferSize;

            //auto vertexLocationPaddingSize = PaddingSize(indexOffset + indexBufferSize, alignment);
            auto vertexLocationPaddingSize = PaddingSize(bufferEndAddress, alignment);
            vertexLocationOffset    = bufferEndAddress + vertexLocationPaddingSize;
            vertexLocationBufferSize = memsize(m_vertexLocations);
            bufferEndAddress = vertexLocationOffset + vertexLocationBufferSize;

            auto indexLocationPaddingSize = PaddingSize(bufferEndAddress, alignment);
            indexLocationOffset    = bufferEndAddress + indexLocationPaddingSize;
            indexLocationBufferSize = memsize(m_indexLocations);
            bufferEndAddress = indexLocationOffset + indexLocationBufferSize;
            
            auto bufferSize = bufferEndAddress; //vertexLocationOffset + vertexLocationBufferSize;
            //auto normPaddingSize = PaddingSize(indexOffset + indexBufferSize, alignment);
            //normalOffset    = indexOffset + indexBufferSize + normPaddingSize;
            //normalBufferSize = memsize(obj.normals());
            //auto bufferSize = normalOffset + normalBufferSize;

            //auto bufferSize = indexOffset + indexBufferSize;

			m_vertexBuffers.emplace_back(CreateVertexBuffer(bufferSize, m_allocator, { raytracingQueue.familyIndex, transferQueue.familyIndex }, true));

			stagingBuffer.LoadData(vertices, indices, m_vertexLocations, m_indexLocations);

			CommandBufferAllocateInfo cmdBufferAllocateInfo{
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1,
			};
			auto commandBuffers = transferCmdPool.AllocateCommandBufferArray(cmdBufferAllocateInfo);
			auto cmdBuf = commandBuffers[0];
			cmdBuf.BeginRecording(+CommandBufferUsageFlagBits::OneTimeSubmit);
			{
				std::vector regions = {
                    BufferCopy2KHR{
					    .srcOffset = 0,
					    .dstOffset = vertexOffset,
					    .size      = vertexBufferSize,
				    },
                    BufferCopy2KHR{
					    .srcOffset = vertexBufferSize,
					    .dstOffset = indexOffset,
					    .size      = indexBufferSize,
				    },
                    BufferCopy2KHR{
					    .srcOffset = vertexBufferSize + indexBufferSize,
					    .dstOffset = vertexLocationOffset,
					    .size      = vertexLocationBufferSize,
				    },
                    BufferCopy2KHR{
					    .srcOffset = vertexBufferSize + indexBufferSize + vertexLocationBufferSize,
					    .dstOffset = indexLocationOffset,
					    .size      = indexLocationBufferSize,
				    },
                    //BufferCopy2KHR{
					//    .srcOffset = vertexBufferSize + indexBufferSize,
					//    .dstOffset = normalOffset,
					//    .size = normalBufferSize,
				    //},
                };
                auto vertexBufferIndex = 0;
				CopyBufferInfo2KHR copyBufferInfo{
					.srcBuffer = stagingBuffer,
					.dstBuffer = m_vertexBuffers[vertexBufferIndex],
				};
				cmdBuf.CopyBuffer(copyBufferInfo, regions);
			}
			cmdBuf.EndRecording();

			VkSubmitInfo2KHR submitInfo{};
            Fence fence(m_device, FenceCreateFlagBits::Unsignaled);
			commandBuffers.SubmitToQueue2KHR(transferQueue, submitInfo, fence);
            fence.Wait();
		}

    // BLAS
    private:
        std::vector<VkAccelerationStructureKHR> blasAccelerationStructures;
        std::vector<BufferVma> blasBuffers;

        auto ObjToGeometry(size_t obj_index,
            out_t<VkAccelerationStructureGeometryKHR>       blasGeometry,
            out_t<VkAccelerationStructureBuildRangeInfoKHR> blasBuildRangeInfo)
            -> void
        {
            auto vertexBufferIndex = 0;

            auto& obj = m_objs[obj_index];
            DeviceAddress vertexAddress = m_vertexBuffers[vertexBufferIndex].GetDeviceAddress();// +m_vertexLocations[obj_index] * sizeof(ObjModel::VertexComponentType);
            DeviceAddress indexAddress = vertexAddress + indexOffset;// +m_indexLocations[obj_index] * sizeof(ObjModel::IndexType);

            blasGeometry = {
                    .sType         {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR},
                    .geometryType  {VK_GEOMETRY_TYPE_TRIANGLES_KHR},
                    .geometry      {
                        .triangles {
                            .sType        {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR},
                            .vertexFormat {VK_FORMAT_R32G32B32_SFLOAT},
                            .vertexData   {.deviceAddress = vertexAddress},
                            .vertexStride {sizeof(ObjModel::TriangleVertexType)},
                            .maxVertex    {static_cast<uint32_t>((m_vertexLocations[obj_index] + obj.vertices().size())/3 - 1)},
                            .indexType    {VK_INDEX_TYPE_UINT32},
                            .indexData    {.deviceAddress = indexAddress },
                        }
                    },
                    .flags         { VK_GEOMETRY_OPAQUE_BIT_KHR},
            };

            blasBuildRangeInfo = {
                .primitiveCount  {static_cast<uint32_t>(obj.indices().size() / 3)},
                .primitiveOffset {static_cast<uint32_t>(m_indexLocations[obj_index] * sizeof(ObjModel::IndexComponentType))},
                .firstVertex     {m_vertexLocations[obj_index] / 3},
                //.primitiveOffset {},
                //.firstVertex     {},
                .transformOffset {0},
            };
        }

        auto PrepareBlasBuildInfo(size_t BLAS_index,
            in_t<VkAccelerationStructureGeometryKHR>       blasGeometry,
            in_t<VkAccelerationStructureBuildRangeInfoKHR> blasBuildRangeInfo,
            out_t<std::vector<BufferVma>>                      blasScratchBuffers,
            out_t<VkAccelerationStructureBuildGeometryInfoKHR> blasBuildGeometryInfo,
            out_t<VkAccelerationStructureKHR>                  blas,
            out_t<std::vector<BufferVma>>                      blasBuffers )
            -> void
        {
            // partial info
            blasBuildGeometryInfo = {
                .sType                    {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR},
                .type                     {VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR},
                .flags                    {VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR}, // TODO: allow update and compactification
                .mode                     {VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR},
                .srcAccelerationStructure {VK_NULL_HANDLE},
                .geometryCount            {1u},
                .pGeometries              {&blasGeometry},
            };
            VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
                .sType {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR},
            };
            vkGetAccelerationStructureBuildSizesKHR(m_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                IN &blasBuildGeometryInfo, IN &blasBuildRangeInfo.primitiveCount, OUT &sizeInfo);

            // Allocate a buffer for the acceleration structure.
            blasBuffers.emplace_back(m_allocator.CreateBufferVma(BufferVmaCreateInfo{
                .bufferCreateInfo {
                    .size  {sizeInfo.accelerationStructureSize},
                    .usage {BufferUsageFlagBits::AccelerationStructureStorageKhr | BufferUsageFlagBits::ShaderDeviceAddress | BufferUsageFlagBits::StorageBuffer},
                },
                .allocationCreateInfo {
                    .usage {+MemoryUsage::GpuOnly},
                },
            }));

            // Create the acceleration structure object. (Data has not yet been set.)
            VkAccelerationStructureCreateInfoKHR createInfo{
                .sType  {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR},
                .buffer {blasBuffers[BLAS_index]},
                .offset {0},
                .size   {sizeInfo.accelerationStructureSize},
                .type   {blasBuildGeometryInfo.type},
            };
            vkCreateAccelerationStructureKHR(m_device, &createInfo, nullptr, OUT &blas) | VK_NO_ERROR;
            blasBuildGeometryInfo.dstAccelerationStructure = blas;

            // Allocate the scratch buffer holding temporary build data.
            blasScratchBuffers.emplace_back(m_allocator.CreateBufferVma(BufferVmaCreateInfo{
                .bufferCreateInfo {
                    .size  {sizeInfo.buildScratchSize},
                    .usage {BufferUsageFlagBits::ShaderDeviceAddress | BufferUsageFlagBits::StorageBuffer},
                },
                .allocationCreateInfo {
                    .usage {+MemoryUsage::GpuOnly},
                },
            }));
            assert(blasScratchBuffers.size() == BLAS_index + 1);
            blasBuildGeometryInfo.scratchData.deviceAddress = blasScratchBuffers[BLAS_index].GetDeviceAddress();
        }

        auto BuildAccelerationStructures(in_t<std::vector<VkAccelerationStructureBuildGeometryInfoKHR>> blasBuildInfos,
                                         in_t<std::vector<VkAccelerationStructureBuildRangeInfoKHR>>    blasBuildRangeInfos)
            -> void
        {
            auto blasCount = blasBuildInfos.size();
            std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> blasBuildRangeInfoPointers(blasCount);
            for (auto i : iota(0uz, blasCount)) {
                blasBuildRangeInfoPointers[i] = &blasBuildRangeInfos[i];
            }
            // TODO: <ranges>
            //std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> accelerationStructureBuildRangeInfoPointers
            //    = accelerationStructureBuildRangeInfos | std::ranges::views::transform(std::addressof);

            CommandBufferArray cmdBuffers = raytracingCmdPool.AllocateSingleCommandBuffer();
            auto cmdBuf = cmdBuffers[0];
            cmdBuf.BeginRecording(+CommandBufferUsageFlagBits::OneTimeSubmit);
            {
			    cmdBuf.BuildAccelerationStructuresKHR(blasCount, blasBuildInfos.data(), blasBuildRangeInfoPointers.data());
            }
            cmdBuf.EndRecording();
            Fence fence(m_device, FenceCreateFlagBits::Unsignaled);
            VkSubmitInfo2KHR submitInfo{ };
            cmdBuffers.SubmitToQueue2KHR(raytracingQueue, submitInfo, fence);
            fence.Wait();
		}

        auto CompatifyBlas() -> void {
        }

    // TLAS
    private:
        std::vector<VkAccelerationStructureKHR> tlasAccelerationStructures;
        std::vector<BufferVma> tlasBuffers;

        auto PrepareInstances(
             in_t<std::vector<VkAccelerationStructureKHR>>     blasArray,
             in_t<std::vector<RayTracingContextCreateInfo::ObjectProperty>> sceneInfo,
             out_t<std::vector<BufferVma>>                     instanceBuffers
        ) -> void
		{
			uint32_t instanceCustomIndex = 0;
			auto blasInstances = std::vector<VkAccelerationStructureInstanceKHR>{};
            auto totalInstancesNum = 0uz;
            for (const auto& objProperty : sceneInfo) {
                totalInstancesNum += objProperty.instanceInfo.count();
            }
            blasInstances.reserve(totalInstancesNum);

			for (auto BLAS_index : iota(0uz, sceneInfo.size())) {

				VkAccelerationStructureDeviceAddressInfoKHR addressInfo{
					.sType                 {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR},
					.accelerationStructure {blasArray[BLAS_index]}
				};
				const DeviceAddress blasAddress = vkGetAccelerationStructureDeviceAddressKHR(m_device, &addressInfo);

				auto blasInstance = VkAccelerationStructureInstanceKHR{
					.mask                                   {0xFF},
					.instanceShaderBindingTableRecordOffset {0},
					.flags                                  {VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR},
					.accelerationStructureReference         {blasAddress},
				};

                auto instanceNum = sceneInfo[BLAS_index].instanceInfo.count();
			    auto instancesOfCurrentBlas = std::vector<VkAccelerationStructureInstanceKHR>(instanceNum, blasInstance);
				for (auto i : iota(0uz, instanceNum)) {
                    const auto& instanceInfo = sceneInfo[BLAS_index].instanceInfo;
					instancesOfCurrentBlas[i].transform = TransformMatrix(instanceInfo.rotates[i], instanceInfo.translates[i]);
                    instancesOfCurrentBlas[i].instanceCustomIndex = BLAS_index;
                    ++instanceCustomIndex;
				}
                blasInstances.insert(blasInstances.end(), instancesOfCurrentBlas.begin(), instancesOfCurrentBlas.end());
			}
            // for debug only
            //std::swap(blasInstances[1], blasInstances[2]);
			instanceBuffers.emplace_back(m_allocator.CreateBufferVma(BufferVmaCreateInfo{
				.bufferCreateInfo {
					//.size  {sizeof(VkAccelerationStructureInstanceKHR)},
					.size  {memsize(blasInstances)},
					.usage {BufferUsageFlagBits::ShaderDeviceAddress | BufferUsageFlagBits::AccelerationStructureBuildInputReadOnlyKhr},
				},
				.allocationCreateInfo {
					.usage {+MemoryUsage::CpuToGpu},
				},
				}));
            // only works for the first TLAS, but currently we don't need more TLASs
			instanceBuffers.back().LoadData(blasInstances);
		}

        //auto PrepareInstances(size_t TLAS_index,
        //    in_t<std::vector<VkAccelerationStructureKHR>> blasArray,
        //    in_t<std::vector<RayTracingContextCreateInfo::InstanceInfo>> instanceInfoArray,
        //    out_t<std::vector<BufferVma>> instanceBuffers)
        //    -> void
        //{
        //    auto blasInstances = std::vector<VkAccelerationStructureInstanceKHR>{};
		//	for (auto BLAS_index : iota(0uz, blasArray.size())) {

		//		VkAccelerationStructureDeviceAddressInfoKHR addressInfo{
		//			.sType                 {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR},
		//			.accelerationStructure {blasArray[BLAS_index]}
		//		};
		//		const DeviceAddress blasAddress = vkGetAccelerationStructureDeviceAddressKHR(m_device, &addressInfo);

		//		auto blasInstance = VkAccelerationStructureInstanceKHR{
		//			.instanceCustomIndex                    {0},
		//			.mask                                   {0xFF},
		//			.instanceShaderBindingTableRecordOffset {0},
		//			.flags                                  {VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR},
		//			.accelerationStructureReference         {blasAddress},
		//		};

        //        const auto& instanceInfo = instanceInfoArray[BLAS_index];
		//		for (auto i : iota(0uz, instanceInfo.count())) {
		//			blasInstance.transform = TransformMatrix(instanceInfo.rotates[i], instanceInfo.translates[i]);
        //            blasInstances.push_back(blasInstance);
		//		}
		//	}
		//    instanceBuffers.emplace_back(m_allocator.CreateBufferVma(BufferVmaCreateInfo{
		//    	.bufferCreateInfo {
		//    		//.size  {sizeof(VkAccelerationStructureInstanceKHR)},
		//    		.size  {memsize(blasInstances)},
		//    		.usage {BufferUsageFlagBits::ShaderDeviceAddress | BufferUsageFlagBits::AccelerationStructureBuildInputReadOnlyKhr},
		//    	},
		//    	.allocationCreateInfo {
		//    		.usage {+MemoryUsage::CpuToGpu},
		//    	},
		//    	}));
        //    instanceBuffers[TLAS_index].LoadData(blasInstances);
        //}

        auto PrepareTlasBuildInfo(size_t TLAS_index,
            in_t<BufferVma> instanceBuffer,
            in_t<size_t>    instanceCount,
            out_t<std::vector<BufferVma>>                      tlasScratchBuffers,
            out_t<VkAccelerationStructureBuildRangeInfoKHR>    buildRangeInfo,
            out_t<VkAccelerationStructureGeometryKHR>          geometry,
            out_t<VkAccelerationStructureBuildGeometryInfoKHR> buildGeometryInfo,
            out_t<VkAccelerationStructureKHR>                  tlas,
            out_t<std::vector<BufferVma>>                      tlasBuffers)
            -> void
		{
            // only works for the first TLAS, but currently we don't need more TLASs
			buildRangeInfo = {
				.primitiveCount  {(uint32_t)instanceCount},
				.primitiveOffset {0u},
				.firstVertex     {0u},
				.transformOffset {0u},
			};
			geometry = {
				.sType        {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR},
				.geometryType {VK_GEOMETRY_TYPE_INSTANCES_KHR},
				.geometry     {
					.instances {
						.sType                {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR},
						.arrayOfPointers      {VK_FALSE},
						.data {.deviceAddress {instanceBuffer.GetDeviceAddress()}, },
					},
				},
			};
			buildGeometryInfo = {
				.sType                    {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR},
				.type                     {VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR},
				.flags                    {VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR},
				.mode                     {VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR},
				.srcAccelerationStructure {VK_NULL_HANDLE},
				.geometryCount            {1u},
				.pGeometries              {&geometry},
			};
            auto sizeInfo = VkAccelerationStructureBuildSizesInfoKHR{ .sType {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR} };
			vkGetAccelerationStructureBuildSizesKHR(m_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                IN &buildGeometryInfo, IN &buildRangeInfo.primitiveCount, OUT &sizeInfo);

            tlasBuffers.emplace_back(m_allocator.CreateBufferVma(BufferVmaCreateInfo{
                .bufferCreateInfo {
                    .size  {sizeInfo.accelerationStructureSize},
                    .usage {BufferUsageFlagBits::AccelerationStructureStorageKhr | BufferUsageFlagBits::ShaderDeviceAddress | BufferUsageFlagBits::StorageBuffer},
                },
                .allocationCreateInfo {
                    .usage {+MemoryUsage::GpuOnly},
                },
            }));

            VkAccelerationStructureCreateInfoKHR createInfo{
                .sType  {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR},
                .buffer {tlasBuffers[TLAS_index]},
                .offset {0},
                .size   {sizeInfo.accelerationStructureSize},
                .type   {buildGeometryInfo.type},
            };
            vkCreateAccelerationStructureKHR(m_device, &createInfo, nullptr, OUT &tlasAccelerationStructures[TLAS_index]) | VK_NO_ERROR;
            buildGeometryInfo.dstAccelerationStructure = tlasAccelerationStructures[TLAS_index];

            // Allocate the scratch buffer holding temporary build data.
            tlasScratchBuffers.emplace_back(m_allocator.CreateBufferVma(BufferVmaCreateInfo{
                .bufferCreateInfo {
                    .size  {sizeInfo.buildScratchSize},
                    .usage {BufferUsageFlagBits::ShaderDeviceAddress | BufferUsageFlagBits::StorageBuffer},
                },
                .allocationCreateInfo {
                    .usage {+MemoryUsage::GpuOnly},
                },
            }));
            assert(tlasScratchBuffers.size() == TLAS_index + 1);
            buildGeometryInfo.scratchData.deviceAddress = tlasScratchBuffers[TLAS_index].GetDeviceAddress();
		}

    public:

        // TODO: update AS
        auto UpdateAccelerationStructure() -> void {
            throw std::runtime_error("Update AS");
        }

        // TODO: clone AS
        auto CloneAccelerationStructure() -> void {
            throw std::runtime_error("Clone AS");
        }

        // TODO: compact AS
        auto CompactAccelerationStructure() -> void {
            throw std::runtime_error("Compact AS");
        }

        template<Array DescriptorSetLayoutContainerType>
            requires std::same_as<typename DescriptorSetLayoutContainerType::value_type, VkDescriptorSetLayout>
        auto CreatePipelineLayout(in_t<DescriptorSetLayoutContainerType> layouts) -> PipelineLayout {
			return PipelineLayout(m_device, PipelineLayoutCreateInfo{
					.setLayoutCount {(uint32_t) layouts.size()},
					.pSetLayouts    {layouts.data()},
				   });
        };

        struct RayTracingShaders {
            ShaderModule aoRgen;
            ShaderModule aoRmiss;
            ShaderModule aoCHitPrimary;
        };

        auto LoadRayTracingShaders() -> RayTracingShaders {
            return RayTracingShaders{
                .aoRgen        {m_device, LoadFile(fs::path("Shaders/ao.rgen.spv"))},
                .aoRmiss       {m_device, LoadFile(fs::path("Shaders/ao.rmiss.spv"))},
                .aoCHitPrimary {m_device, LoadFile(fs::path("Shaders/ao_primary.rchit.spv"))},
            };
        }

        struct ShaderStages {
            std::vector<VkPipelineShaderStageCreateInfo> m_stages;
            auto size() const -> uint32_t {
                return (uint32_t)m_stages.size();
            }
            auto data() -> VkPipelineShaderStageCreateInfo* {
                return m_stages.data();
            }
            auto data() const -> VkPipelineShaderStageCreateInfo const* {
                return m_stages.data();
            }
        };
        struct ShaderGroups {
            std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_groups;
            auto size() const -> uint32_t {
                return (uint32_t)m_groups.size();
            }
            auto data() -> VkRayTracingShaderGroupCreateInfoKHR* {
                return m_groups.data();
            }
            auto data() const -> VkRayTracingShaderGroupCreateInfoKHR const* {
                return m_groups.data();
            }
        };

        struct RaytracingPipelineCreateInfo {
            in_t<RayTracingShaders> shaders;
            in_t<PipelineLayout> pipelineLayout;
            in_t<ShaderStages> stages;
            in_t<ShaderGroups> groups;
        };

        class RayTracingPipeline {
            VkDevice m_device;
            VkPipeline m_pipeline;
            VkAllocationCallbacks const* m_allocator;
        public:
            VK_NO_COPY_NO_DEFAULT(RayTracingPipeline);
			RayTracingPipeline(in_t<Device> device, in_t<RaytracingPipelineCreateInfo> info, VkAllocationCallbacks const* allocator = default_allocator)
                : m_device(device), m_allocator{ allocator } {
				VkRayTracingPipelineCreateInfoKHR pipelineCreateInfo{
                    .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
				    .flags = 0,  // No flags to set
				    .stageCount = info.stages.size(),
				    .pStages    = info.stages.data(),
				    .groupCount = info.groups.size(),
				    .pGroups    = info.groups.data(),
				    .maxPipelineRayRecursionDepth = 1,  // Depth of call tree
				    .layout     = info.pipelineLayout,
                };
				vkCreateRayTracingPipelinesKHR(m_device,                  // Device
					VK_NULL_HANDLE,          // Deferred operation or VK_NULL_HANDLE
					VK_NULL_HANDLE,          // Pipeline cache or VK_NULL_HANDLE
			    IN  1, &pipelineCreateInfo,  // Array of create infos
					nullptr,                 // Allocator
			    OUT &m_pipeline) | VK_NO_ERROR;
			}
            ~RayTracingPipeline() {
                if (m_pipeline != null_handle) {
                    vkDestroyPipeline(m_device, m_pipeline, m_allocator);
                    m_pipeline = null_handle;
                }
            }
            operator VkPipeline() const { return m_pipeline; }
        };

        //auto CreatePipeline() -> RayTracingPipeline {
        //    return RayTracingPipeline(m_device, RaytracingPipelineCreateInfo{});
        //}

        auto& GetAccelerationStructures() {
            return tlasAccelerationStructures;
        }
        const auto& GetAccelerationStructures() const {
            return tlasAccelerationStructures;
        }

        auto GetVertexBuffer(size_t index = 0) const -> VkBuffer {
            return (VkBuffer) m_vertexBuffers[index];
        }

        auto GetVertexSize  () const -> DeviceSize {
            return vertexBufferSize;
        }
        auto GetVertexOffset() const -> DeviceSize {
            return vertexOffset;
        }
        auto GetIndexSize   () const -> DeviceSize {
            return indexBufferSize;
        }
        auto GetIndexOffset () const -> DeviceSize {
            return indexOffset;
        }
        auto GetVertexLocationSize  () const -> DeviceSize {
            return vertexLocationBufferSize;
        }
        auto GetVertexLocationOffset() const -> DeviceSize {
            return vertexLocationOffset;
        }
        auto GetIndexLocationSize  () const -> DeviceSize {
            return indexLocationBufferSize;
        }
        auto GetIndexLocationOffset() const -> DeviceSize {
            return indexLocationOffset;
        }
        //auto GetNormalSize  () const -> DeviceSize {
        //    return normalBufferSize;
        //}
        //auto GetNormalOffset() const -> DeviceSize {
        //    return normalOffset;
        //}
    public:
        VK_DELETE_ALL_DEFAULT(RayTracingContext);
        RayTracingContext(RayTracingContextCreateInfo& info) :
            m_device          {info.device},
            transferQueue     {m_device.m_queueT},
            raytracingQueue   {m_device.m_queueCT},
            m_allocator       {info.allocator},
            transferCmdPool   {info.transferCmdPool},
            raytracingCmdPool {info.raytracingCmdPool}
        {
            LogInfo("\n==========\n");
            LogInfo("Initializing ray tracing context...\n");


            LoadObjs(info.sceneInfo);
            InitRayTracing();

            auto objectsCount = m_objs.size();
            m_vertexBuffers.reserve(objectsCount);
            //for (auto obj_index : iota(0uz, objectsCount)) {
            //    //LogInfo("Loading obj #{}\n", obj_index);
            //    LoadObjsToBuffers(obj_index, info.stagingBuffer);
            //}
            LoadObjsToBuffers(info.stagingBuffer);

            // building BLAS

            std::vector<VkAccelerationStructureGeometryKHR>          blasGeometries        (objectsCount);
            std::vector<VkAccelerationStructureBuildRangeInfoKHR>    blasBuildRangeInfos   (objectsCount);
            std::vector<VkAccelerationStructureBuildGeometryInfoKHR> blasBuildGeometryInfos(objectsCount);
            
            std::vector<BufferVma> scratchBuffers;
            scratchBuffers.reserve(objectsCount);
            blasAccelerationStructures.resize(objectsCount);
            blasBuffers.reserve(objectsCount);
            for (auto obj_index : iota(0uz, objectsCount)) {
                //LogInfo("Building geometry for obj #{}\n", obj_index);
                ObjToGeometry(obj_index,
                    OUT blasGeometries[obj_index], OUT blasBuildRangeInfos[obj_index]);
                PrepareBlasBuildInfo(obj_index,
                    IN blasGeometries[obj_index], IN blasBuildRangeInfos[obj_index],
                    OUT scratchBuffers, OUT blasBuildGeometryInfos[obj_index], OUT blasAccelerationStructures[obj_index], OUT blasBuffers);
            }
            BuildAccelerationStructures(IN blasBuildGeometryInfos, IN blasBuildRangeInfos); // OUT: GPU writes data into accelerationStructureBuffers

            scratchBuffers.clear();
            // build TLAS

            auto tlasCount = 1uz;
            std::vector<BufferVma> instanceBuffers;

            auto instanceCount = 0uz;
            for (const auto& objInfo : info.sceneInfo) {
                assert(objInfo.instanceInfo.rotates.size() == objInfo.instanceInfo.translates.size());
                instanceCount += objInfo.instanceInfo.count();
            }
            LogInfo("Total instances: {}\n", instanceCount);

            instanceBuffers.reserve(tlasCount);
            PrepareInstances(IN blasAccelerationStructures, IN info.sceneInfo, OUT instanceBuffers);

            std::vector<VkAccelerationStructureGeometryKHR>          tlasGeometries(tlasCount);
            std::vector<VkAccelerationStructureBuildRangeInfoKHR>    tlasBuildRangeInfos(tlasCount);
            std::vector<VkAccelerationStructureBuildGeometryInfoKHR> tlasBuildInfos(tlasCount);
            tlasAccelerationStructures.resize(tlasCount);
            tlasBuffers.reserve(tlasCount);
            scratchBuffers.reserve(tlasCount);
            for (auto tlas_index : iota(0uz, tlasCount)) {
                PrepareTlasBuildInfo(tlas_index,
                    IN instanceBuffers[tlas_index], IN instanceCount,
                    OUT scratchBuffers, OUT tlasBuildRangeInfos[tlas_index], OUT tlasGeometries[tlas_index], OUT tlasBuildInfos[tlas_index],
                    OUT tlasAccelerationStructures[tlas_index], OUT tlasBuffers);
            }
            BuildAccelerationStructures(IN tlasBuildInfos, IN tlasBuildRangeInfos);

            LogInfo("Acceleration structures have been built. Models available for ray tracing:\n");
            //for (auto& filename : m_filenames) {
            for (auto i : iota(0uz, objectsCount)) {
                LogColorfulInfo(fmt::color::light_green, "---- {}\n", m_filenames[i]);
                LogColorfulInfo(fmt::color::light_green, "-------- Instance count: {}\n", info.sceneInfo[i].instanceInfo.count());
            }
            LogInfo("\n==========\n");

        }
        ~RayTracingContext() {
            for (auto accelerationStructure : blasAccelerationStructures) {
                vkDestroyAccelerationStructureKHR(m_device, accelerationStructure, default_allocator);
            }
            for (auto accelerationStructure : tlasAccelerationStructures) {
                vkDestroyAccelerationStructureKHR(m_device, accelerationStructure, default_allocator);
            }
        }

    };

    // ImGui
    //
    // TODO: move to a separate module partition

    auto InitImGui() -> void {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsClassic();
    }


    // TODO: move to Sources/Application.ixx and merge with the physics module
	struct ApplicationCreateInfo {
		InstanceCreateInfo	                instanceCreateInfo;
		DeviceCreateInfo	                deviceCreateInfo;
		glfw::WindowCreateInfo              windowCreateInfo;
		std::vector<std::string>            shaderSources;
		int windowWidth;
		int windowHeight;

		auto enableValidationLayer() {
			instanceCreateInfo.enableValidationLayer();
			//deviceCreateInfo.enable...?
		}
	};

    //
    // #Application
    //

	class Application {
		glfw::GlfwVulkanContext glfwVulkanContext;
		Instance     instance;
		glfw::Window window;
		Surface      surface;
		Device       device;
        ResourceAllocator allocator;
        const std::size_t maxFramesInFlight;
        std::size_t       currentFrame;
        double time;

#ifdef RASTERIZATION_ON
		ShaderSPIRV  spirv;
        bool enableRotation;
        glm::mat4 cameraTransform;
        glm::vec3 lookAtCenter;
        glm::vec3 lookAtEye;
#endif
        glm::mat4 rtCameraTransform;
        glm::mat4 rtCameraRayTransform;
        glm::vec3 faceDir;
        glm::vec3 topDir;
        glm::float32 steplength;
	public:
		VK_DELETE_ALL_DEFAULT(Application);
		// Intellisense might complain several times, which should be a bug
		// info.deviceCreateInfo.checkers will be modified
		explicit Application(ApplicationCreateInfo& info) :
                glfwVulkanContext (),
                instance          (info.instanceCreateInfo),
                window            (info.windowCreateInfo, &info.windowWidth, &info.windowHeight),
                surface           (instance, window.CreateSurface(instance, default_allocator), info.deviceCreateInfo.checkers, info.windowWidth, info.windowHeight),
                device            (info.deviceCreateInfo, instance, default_allocator),
                allocator         (device, +AllocatorCreateFlagBits::BufferDeviceAddress),
                maxFramesInFlight {2uz},
                currentFrame      {0uz},
                time              {glfw::GetTime()},
#ifdef RASTERIZATION_ON
                spirv             {},
                enableRotation    {false},
                cameraTransform   (), // identity matrix
                lookAtCenter      (),
                lookAtEye         (2.0f, 2.0f, 2.0f),
#endif
                rtCameraTransform   (1.0f),
                rtCameraRayTransform(1.0f),
                faceDir             (0.0f, 0.0f, -1.0f),
                topDir              (0.0f, 1.0f, 0.0f),
                steplength          {0.1f}
        {
            window.SetWindowUserPointer(this);
            //LogInfo("App start:\n");
            raytracing_app();
        }

        // TODO: multi-threaded rendering
        // rasterization


        //struct UniformBufferObject {
        //    glm::float32 time;
        //    alignas(8) glm::vec2 resolution;
        //    alignas(16) glm::mat4 model;
        //    glm::mat4 view;
        //    glm::mat4 proj;
        //};

        struct RaytracingUBO {
            alignas(4)  glm::float32 time;
            alignas(16) glm::mat4 cameraTransform;
            alignas(16) glm::mat4 cameraRayTransform;
        };


        template<typename UBType>
        auto CreateUniformBuffers(size_t count) {
            std::vector<BufferVma> uniformBuffers;
            BufferVmaCreateInfo uniformBufferVmaCreateInfo{
                .bufferCreateInfo {
                    .size = sizeof(UBType),
                    .usage = +BufferUsageFlagBits::UniformBuffer,
                },
                .allocationCreateInfo {
                    .usage = +MemoryUsage::CpuToGpu,
                }
            };
            for (auto i : iota(0uz, count)) {
                uniformBuffers.push_back(allocator.CreateBufferVma(uniformBufferVmaCreateInfo));
            }
            return uniformBuffers;
        }

        struct TextureUploadingContext {
            in_t<fs::path> texturePath;
            inout_t<StagingBuffer> stagingBuffer;
            in_t<CommandPool> cmdPool;
            in_t<Queue> queue;
        };
#if 0
		auto CreateTextureImage(in_t<fs::path> texturePath, inout_t<StagingBuffer> stagingBuffer) {

			StbImage textureStbImage(texturePath);
			stagingBuffer.LoadTexture(textureStbImage);

            auto imageInfo = ImageVmaCreateInfo{
.imageCreateInfo {
.imageType {},
},
.allocationCreateInfo {
}
            };



        CommandBufferAllocateInfo cmdBufferAllocateInfo {
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        auto commandBuffers = context.pool.AllocateCommandBufferArray(cmdBufferAllocateInfo);
        auto cmdBuf = context.commandBuffers[0];
        cmdBuf.BeginRecording(+CommandBufferUsageFlagBits::OneTimeSubmit);
        {
            std::vector regions = { BufferCopy2KHR{
                .srcOffset = 0,
                .dstOffset = 0,
                .size = context.bufferSize,
            } };
            CopyBufferInfo2KHR copyBufferInfo{
                .srcBuffer = context.stagingBuffer,
                .dstBuffer = context.vertexBuffer,
            };
            cmdBuf.CopyBuffer(copyBufferInfo, regions);
        }
        cmdBuf.EndRecording();
    
        VkSubmitInfo2KHR submitInfo{};
        context.commandBuffers.SubmitToQueue2KHR(context.queue, submitInfo, context.fence);

        return textureImage;
		}

#endif
		auto rotate_x(float angle) {
			return glm::rotate(glm::mat4(1.0f), angle, glm::vec3(1, 0, 0));
		}
		auto rotate_y(float angle) {
			return glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0, 1, 0));
		}
		auto rotate_z(float angle) {
			return glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0, 0, 1));
		}
        auto rotate_x_then_y(float angle_x, float angle_y) {
            return glm::rotate(rotate_x(angle_x), angle_y, glm::vec3(0, 1, 0));
        }

        void raytracing_app() {
            /*
            ** ASSET_PATH is defined in CMakeLists.text
            ** other macros are not neccesasry, use those std::filesystem::path instead
            */
            const auto asset_path         = fs::path(ASSET_PATH);
            const auto shader_source_path = asset_path / "Shaders";
            const auto model_path         = asset_path / "Models";
            const auto texture_path       = asset_path / "Textures";
            /*
            ** They might looks weird under non-English Windows.
            */
            LogWarning("Rasterization App Start...\n");
            LogWarning("----Assets  : {}\n", asset_path.string());
            LogWarning("----Shaders : {}\n", shader_source_path.string());
            LogWarning("----Models  : {}\n", model_path.string());
            LogWarning("----Textures: {}\n", texture_path.string());
            LogWarning("----Shader bytecodes : ${{DirectoryOfBin}}/Shaders/${{ShaderSourceFileName}}.spv\n");


            auto graphicsQueue   = device.m_queueGCT;
            auto raytracingQueue = device.m_queueCT;
            auto transferQueue   = device.m_queueT;

            CommandPool graphicsCommandPool  (device, CommandPoolCreateInfo{ .queueFamilyIndex = graphicsQueue.familyIndex   });
            CommandPool graphicsShortLivingCommandPool  (device,
                CommandPoolCreateInfo{
                    .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                    .queueFamilyIndex = graphicsQueue.familyIndex
                });
            CommandPool raytracingCommandPool(device, CommandPoolCreateInfo{ .queueFamilyIndex = raytracingQueue.familyIndex });
            CommandPool transferCommandPool  (device,
                                              CommandPoolCreateInfo{
                                                  .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                                                  .queueFamilyIndex = transferQueue.familyIndex,
                                              } );

            StagingBuffer stagingBuffer(allocator, transferQueue);

#ifdef RASTERIZATION_ON
            const VertexData vertexData{
				.vertices {
				    {{-0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}},
				    {{ 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
				    {{ 0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}},
				    {{-0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
				},
                .indices  {
	                0, 1, 2,
                    2, 3, 0,
                },
            };




            // texture
            //auto texture_statue = CreateTextureImage(texture_path / "statue.jpg", stagingBuffer);

            // rasterization
            auto vertexBuffer = CreateVertexBuffer(vertexData, allocator, {graphicsQueue.familyIndex, transferQueue.familyIndex});
            //LogWarning("Vertex buffer address: {}\n", (uint64_t)static_cast<VkBuffer>(vertexBuffer));

            //auto [vertexBufferVma, stagingBufferVma] = CreateVertexAndStagingBuffer(vertexData, allocator, {graphicsQueue.familyIndex, transferQueue.familyIndex});

            MeshUploadingContext meshUploadingContext{
                .fence         {Fence(device, FenceCreateFlagBits::Unsignaled)},
                .queue         {transferQueue},
                .pool          {transferCommandPool},
                .commandBuffers{CommandBufferArray(device, transferCommandPool)},
                .vertexBuffer  {vertexBuffer},
                .stagingBuffer {stagingBuffer},
                .vertexData    {vertexData},
                .bufferSize    {memsize(vertexData.vertices) + memsize(vertexData.indices)},
            };
            std::thread ioThread(UploadMesh, std::ref(meshUploadingContext));
            ioThread.detach();
#endif

            constexpr auto quarter_pi = glm::quarter_pi<float>();
            constexpr auto half_pi = glm::half_pi<float>();
            constexpr auto pi = glm::pi<float>();

            auto rtStagingBuffer = StagingBuffer(allocator, transferQueue);
            // #Scene
            auto rtCreateInfo = RayTracingContextCreateInfo {
                .device {device},
                .sceneInfo {
                    {
                        .pathToModel = fs::path(MODEL_PATH "/Medieval_building_flat_faces.obj"),
                        .instanceInfo = {
                            .rotates = {
                                rotate_y(quarter_pi),
                                rotate_y(-quarter_pi),
                            },
                            .translates = {
                                glm::vec3(-2.5f, 0.0f, -2.0f),
                                glm::vec3(+2.5f, 0.0f, -2.0f),
                            },
                        }
                    },
                    {
                        .pathToModel = fs::path(MODEL_PATH "/plane.obj"),
                        .instanceInfo = {
                            .rotates = {
                                glm::mat3(1.0f),
                            },
                            .translates = {
                                glm::vec3(0.0f, 0.0f, 0.0f),
                            }
                        }
                    },
                    {
                        .pathToModel = fs::path(MODEL_PATH "/stanford-bunny.obj"),
                        .instanceInfo = {
                            .rotates = {
                                4.0f * rotate_y(quarter_pi),
                                4.0f * rotate_y(-quarter_pi),
                            },
                            .translates = {
                                glm::vec3(-0.5f, 0.0f, 1.0f) - glm::vec3(0.0f, 0.2f, 0.0f),
                                glm::vec3(+0.5f, 0.0f, 2.5f) - glm::vec3(0.0f, 0.2f, 0.0f),
                            }
                        }
                    },
                    {
                        .pathToModel = fs::path(MODEL_PATH "/cow.obj"),
                        .instanceInfo = {
                            .rotates = {
                                0.1f * rotate_y(quarter_pi),
                                0.1f * rotate_y(-quarter_pi),
                            },
                            .translates = {
                                glm::vec3(1.5f, 0.5f, 1.0f) - glm::vec3(0.0f, 0.2f, 0.0f),
                                glm::vec3(2.5f, 0.5f, 2.5f) - glm::vec3(0.0f, 0.2f, 0.0f),
                            }
                        }
                    },
                    {
                        .pathToModel = fs::path(MODEL_PATH "/horse.obj"),
                        .instanceInfo = {
                            .rotates = {
                                4.0f * rotate_x_then_y(half_pi, pi + quarter_pi),
                                4.0f * rotate_x_then_y(half_pi, pi - quarter_pi),
                            },
                            .translates = {
                                glm::vec3(-2.5f, 0.5f, 1.0f) - glm::vec3(0.0f, 0.2f, 0.0f),
                                glm::vec3(-1.5f, 0.5f, 2.5f) - glm::vec3(0.0f, 0.2f, 0.0f),
                            }
                        }
                    },
                    {
                        .pathToModel = fs::path(MODEL_PATH "/lucy.obj"),
                        .instanceInfo = {
                            .rotates = {
                                0.002f * rotate_x_then_y(half_pi, +half_pi),
                                0.002f * rotate_x_then_y(half_pi, -half_pi),
                            },
                            .translates = {
                                glm::vec3(-1.5f, 0.5f, 3.0f) + glm::vec3(0.0f, 0.4f, 0.0f),
                                glm::vec3(+3.5f, 0.5f, 4.0f) + glm::vec3(0.0f, 0.4f, 0.0f),
                            }
                        }
                    },
                },
                .stagingBuffer     {rtStagingBuffer},
                .allocator         {allocator},
                .transferCmdPool   {transferCommandPool},
                .raytracingCmdPool {raytracingCommandPool},
            };
            RayTracingContext rtContext(rtCreateInfo);
            // goto here if window size changed (and the previous swapchain becomes invalid
        recreate_swapchain:
            window.UpdateWindowSize();
            while (window.isMinimized()) {
                window.UpdateWindowSize();
                window.WaitEvents();
            }

            constexpr bool enable_raytracing = true;
		    Swapchain swapchain(device, surface, enable_raytracing);

			auto swapchainImageViews = swapchain.getImageViews();

#ifdef RASTERIZATION_ON

#pragma region Descriptor Set
            std::array uboLayoutBindings{
                VkDescriptorSetLayoutBinding {
                    .binding         {0u},
                    .descriptorType  {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
                    .descriptorCount {1u},
                    .stageFlags      {+ShaderStageFlagBits::Vertex},
                    .pImmutableSamplers{},
                },
            };
            DescriptorSetLayoutCreateInfo layoutInfo(uboLayoutBindings);
            DescriptorSetLayout descriptorSetLayout(device, layoutInfo);
            std::array descriptorSetLayoutArray = { (VkDescriptorSetLayout)descriptorSetLayout};

#pragma endregion Descriptor Set

#pragma region Push Constants
            struct PushConstants {
                glm::float32 time;
            } pushConstant{};
            static_assert(sizeof(pushConstant) % 4 == 0);

			std::array<VkPushConstantRange, 0> pushConstantRanges{
                //VkPushConstantRange {
				//    .stageFlags = +ShaderStageFlagBits::Fragment,
				//    .size = sizeof(pushConstant),
			    //}
			};
#pragma endregion Push Constants
            //
            // fill graphicsPipeline layout info, render pass info, and hence graphicsPipeline create info
            //
#pragma region Create graphics pipeline
            PipelineLayoutCreateInfo pipelineLayoutInfo{
                .setLayoutCount         {descriptorSetLayoutArray.size()},
                .pSetLayouts            {descriptorSetLayoutArray.data()},
                .pushConstantRangeCount {pushConstantRanges.size()}, // Optional
                .pPushConstantRanges    {pushConstantRanges.data()}, // Optional
            };

            VkAttachmentDescription colorAttachment{
                .format         = swapchain.imageFormat(),
                .samples        = VK_SAMPLE_COUNT_1_BIT,
                .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            };
            VkAttachmentReference colorAttachmentRef{
                .attachment = 0,
                .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };
            VkSubpassDescription subpass{
                .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = 1,
                .pColorAttachments    = &colorAttachmentRef,
            };
            // TODO: depth attachment
            VkSubpassDependency dependency = {
                .srcSubpass    = VK_SUBPASS_EXTERNAL,
                .dstSubpass    = 0,
                .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,//VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                //.dependencyFlags = 0,
            };

            RenderPassCreateInfo renderPassInfo{
                .attachments  = {colorAttachment},
                .subpasses    = {subpass},
                .dependencies = {dependency},
            };

            auto vertexInputBindingDescriptions   = Vertex::BindingDescriptions();
            auto vertexInputAttributeDescriptions = Vertex::AttributeDescriptions();
            PipelineCreateInfo pipelineCreateInfo{
                .spirv              = spirv,
                .swapchain          = swapchain,
                .pipelineLayoutInfo = pipelineLayoutInfo,
                .renderPassInfo     = renderPassInfo,
                .vertexInputBindingDescriptions   = vertexInputBindingDescriptions,
                .vertexInputAttributeDescriptions = vertexInputAttributeDescriptions,
            };
            GraphicsPipeline graphicsPipeline(device, pipelineCreateInfo);
#pragma endregion Create graphics pipeline

#pragma region Create framebuffers
            std::vector<Framebuffer> swapchainFramebuffers;
			for (const auto& imageView : swapchainImageViews) {
				swapchainFramebuffers.push_back(createFramebuffer(graphicsPipeline.getRenderPass(), imageView, swapchain));
			}
#pragma endregion Create framebuffers

#pragma region Uniform Buffers
            auto uniformBuffers = CreateUniformBuffers<UniformBufferObject>(swapchainImageViews.size());
#pragma endregion Uniform Buffers

#pragma region Descriptor Pool & Sets
            std::array descriptorPoolSizes {
                VkDescriptorPoolSize {
                    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = static_cast<uint32_t>(swapchainImageViews.size()),
                }
            };
            DescriptorPoolCreateInfo descriptorPoolCreateInfo{
                .maxSets       = static_cast<uint32_t>(swapchainImageViews.size()),
                .poolSizeCount = descriptorPoolSizes.size(),
                .pPoolSizes    = descriptorPoolSizes.data(),
            };
            DescriptorPool descriptorPool(device, descriptorPoolCreateInfo);

            std::vector<VkDescriptorSetLayout> descriptorSetLayouts(swapchainImageViews.size(), descriptorSetLayout);

            auto descriptorSets = descriptorPool.AllocateDescriptorSets(descriptorSetLayouts);

			for (auto i : iota(0uz, swapchainImageViews.size())) {
				VkDescriptorBufferInfo bufferInfo{
				    .buffer = uniformBuffers[i],
				    .offset = 0,
				    .range = sizeof(UniformBufferObject),
                };

				VkWriteDescriptorSet descriptorWrite{
				    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				    .dstSet = descriptorSets[i],
				    .dstBinding = 0,
				    .dstArrayElement = 0,
				    .descriptorCount = 1,
				    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .pImageInfo = nullptr,
				    .pBufferInfo = &bufferInfo,
                    .pTexelBufferView = nullptr,
				};

				vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
			}

#pragma endregion Descriptor Pool & Sets

#pragma region Allocate&Record Command Buffer

            //CommandBufferAllocateInfo pushConstantCommandBufferAllocateInfo{
            //    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            //    .commandBufferCount = 1u,
            //};
            //auto pushConstantCommandBuffers = graphicsShortLivingCommandPool.AllocateCommandBufferArray(pushConstantCommandBufferAllocateInfo);

            CommandBufferAllocateInfo cmdBufferAllocateInfo {
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = static_cast<uint32_t>(swapchainFramebuffers.size()),
            };
            auto commandBuffers = graphicsCommandPool.AllocateCommandBufferArray(cmdBufferAllocateInfo);

            for (auto i : iota(0uz, commandBuffers.size())) {

                //pushConstantCommandBuffers.push_back(graphicsCommandPool.AllocateCommandBufferArray(pushConstantCommandBufferAllocateInfo));
                auto cmdBuf = commandBuffers[i];
                cmdBuf.BeginRecording();
				{
					std::array clearValues = {
						VkClearValue {
							.color = {
								.float32 = {0.0f, 0.0f, 0.0f, 1.0f}, // black
							}
						},
					};
					RenderPassBeginInfo renderPassBeginInfo{
						.renderPass = graphicsPipeline.getRenderPass(),
						.framebuffer = swapchainFramebuffers[i],
						.renderArea {
							.offset {
								.x = 0,
								.y = 0,
							},
							.extent = swapchain.extent(),
						},
						.clearValueCount = clearValues.size(),
						.pClearValues = clearValues.data(),
					};
					cmdBuf.BeginRenderPass(renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

					cmdBuf.BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

                    std::array vertexBuffers = { (VkBuffer)vertexBuffer };
                    std::array offsets       = { (DeviceSize)0 };
                    cmdBuf.BindVertexBuffers(0, vertexBuffers.size(), vertexBuffers.data(), offsets.data());
                    cmdBuf.BindIndexBuffers(vertexBuffer, memsize(vertexData.vertices));

                    std::array descriptorSetsToBind{ descriptorSets[i] };
                    cmdBuf.BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.getLayout(), 0, descriptorSetsToBind);

                    cmdBuf.DrawIndexed(static_cast<uint32_t>(vertexData.indices.size()), 1u, 0u, 0u, 0u);
                    //cmdBuf.ExecuteCommandBuffers(pushConstantCommandBuffers[i]);

					cmdBuf.EndRenderPass();

                    // pipeline barrier
				}
                cmdBuf.EndRecording();
            }
#pragma endregion Allocate&Record Command Buffer
#endif


#pragma region raytracing
            auto rtUniformBuffers = CreateUniformBuffers<RaytracingUBO>(swapchainImageViews.size());

            auto rtImages = std::vector<ImageVma>{};
            rtImages.reserve(swapchainImageViews.size());
            auto rtImageViews = std::vector<ImageView>{};
            rtImageViews.reserve(swapchainImageViews.size());

            constexpr auto bindingNum = 7;

            auto bindings = std::array {
                VkDescriptorSetLayoutBinding {
			        .binding = BINDING_IMAGE_AO,
			        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			        .descriptorCount = 1,
                    .stageFlags = +ShaderStageFlagBits::RaygenKhr,
                },
                VkDescriptorSetLayoutBinding {
			        .binding = BINDING_TLAS,
			        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
			        .descriptorCount = 1,
			        .stageFlags = +ShaderStageFlagBits::RaygenKhr,
                },
                VkDescriptorSetLayoutBinding {
                    .binding = BINDING_UNIFORM,
			        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			        .descriptorCount = 1,
			        .stageFlags = +ShaderStageFlagBits::RaygenKhr,
                },
                VkDescriptorSetLayoutBinding {
			        .binding = BINDING_VERTICES,
			        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			        .descriptorCount = 1,
			        .stageFlags = +ShaderStageFlagBits::ClosestHitKhr,
                },
                VkDescriptorSetLayoutBinding {
			        .binding = BINDING_INDICES,
			        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			        .descriptorCount = 1,
			        .stageFlags = +ShaderStageFlagBits::ClosestHitKhr,
                },
                VkDescriptorSetLayoutBinding {
			        .binding = BINDING_VERTEX_LOCATIONS,
			        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			        .descriptorCount = 1,
			        .stageFlags = +ShaderStageFlagBits::ClosestHitKhr,
                },
                VkDescriptorSetLayoutBinding {
			        .binding = BINDING_INDEX_LOCATIONS,
			        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			        .descriptorCount = 1,
			        .stageFlags = +ShaderStageFlagBits::ClosestHitKhr,
                },
                //VkDescriptorSetLayoutBinding {
			    //    .binding = BINDING_NORMALS,
			    //    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			    //    .descriptorCount = 1,
			    //    .stageFlags = +ShaderStageFlagBits::ClosestHitKhr,
                //},
            };
            static_assert(bindings.size() == bindingNum);

            auto rtSetLayoutInfo  = DescriptorSetLayoutCreateInfo(bindings);
            auto rtSetLayout      = DescriptorSetLayout(device, rtSetLayoutInfo);
            auto rtSetLayoutArray = std::array{(VkDescriptorSetLayout) rtSetLayout};

            auto rtPipelineLayout = PipelineLayout(device, PipelineLayoutCreateInfo{
                .setLayoutCount = rtSetLayoutArray.size(),
                .pSetLayouts    = rtSetLayoutArray.data(),
            });


            auto poolSizes = std::array{
                VkDescriptorPoolSize {
                    .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    .descriptorCount = 1,
                },
                VkDescriptorPoolSize {
                    .type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                    .descriptorCount = 1,
                },
                VkDescriptorPoolSize {
                    .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .descriptorCount = bindingNum - 1 - 1 - 1,
                },
                VkDescriptorPoolSize {
                    .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 1,
                },
            };

            auto descriptorPoolInfo = DescriptorPoolCreateInfo{
                .maxSets       {(uint32_t)swapchainImageViews.size()},
                .poolSizeCount {poolSizes.size()},
                .pPoolSizes    {poolSizes.data()},
            };
            auto rtDescriptorPool = DescriptorPool(device, descriptorPoolInfo);
            auto rtDescriptorSetLayouts = std::vector(swapchainImageViews.size(), (VkDescriptorSetLayout)rtSetLayout);
            auto rtDescriptorSetArray = rtDescriptorPool.AllocateDescriptorSets(rtDescriptorSetLayouts);

			for (auto i : iota(0uz, swapchainImageViews.size())) {
                auto format = VK_FORMAT_B8G8R8A8_UNORM;

                rtImages.emplace_back(allocator.CreateImageVma(ImageVmaCreateInfo{
                        .imageCreateInfo {
                            .format = format,
                            .extent = Extent2Dto3D(swapchain.extent()),
                            .tiling = VK_IMAGE_TILING_OPTIMAL,
                            .usage  = {ImageUsageFlagBits::TransferSrc | ImageUsageFlagBits::Storage},
                        },
                        .allocationCreateInfo {
                            .usage = +MemoryUsage::GpuOnly,
                        },
                    }));
                LogInfo("Image[{}] ID: {}\n", i, (void*)rtImages[i]);

                rtImageViews.emplace_back(device, ImageViewCreateInfo{
                        .image  = rtImages[i],
                        .format = format,
                        .subresourceRange {
                            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                            .baseMipLevel   = 0,
                            .levelCount     = 1,
                            .baseArrayLayer = 0,
                            .layerCount     = 1
                        }
                    });
                LogInfo("ImageView[{}] ID: {}\n", i, (void*)rtImageViews[i]);

				auto rtDescriptorSet = rtDescriptorSetArray[i];

				std::array<VkWriteDescriptorSet, bindingNum> writeDescriptorSets{};

                auto write_index = 0uz;
				// Ambient occlusion image, to be presented on the display
				VkDescriptorImageInfo descriptorImageAOInfo{
					.imageView = rtImageViews[i], //imageAOView;
					.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
				};
                writeDescriptorSets[write_index++] = {
				    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				    .dstSet = rtDescriptorSet,
				    .dstBinding = BINDING_IMAGE_AO,
				    .descriptorCount = 1,
				    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				    .pImageInfo = &descriptorImageAOInfo,
                };

				// TLAS
				VkWriteDescriptorSetAccelerationStructureKHR descriptorAS{
					.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
					.accelerationStructureCount = (uint32_t) rtContext.GetAccelerationStructures().size(),
					.pAccelerationStructures    = rtContext.GetAccelerationStructures().data(),
				};
                writeDescriptorSets[write_index++] = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = &descriptorAS,
                    .dstSet = rtDescriptorSet,
                    .dstBinding = BINDING_TLAS,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                };

                // uniform buffer
                VkDescriptorBufferInfo uniformBufferInfo{
				    .buffer = rtUniformBuffers[i],
				    .offset = 0,
				    .range = sizeof(RaytracingUBO),
                };
				writeDescriptorSets[write_index++] = {
				    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				    .dstSet = rtDescriptorSet,
				    .dstBinding = BINDING_UNIFORM,
				    .descriptorCount = 1,
				    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				    .pBufferInfo = &uniformBufferInfo,
				};

				// Vertex buffer
				VkDescriptorBufferInfo descriptorVertexInfo{
				    .buffer = rtContext.GetVertexBuffer(),
				    .offset = rtContext.GetVertexOffset(),
				    .range  = rtContext.GetVertexSize(),
                };
                writeDescriptorSets[write_index++] = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				    .dstSet = rtDescriptorSet,
				    .dstBinding = BINDING_VERTICES,
				    .descriptorCount = 1,
				    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				    .pBufferInfo = &descriptorVertexInfo,
                };

				// Index buffer
                VkDescriptorBufferInfo descriptorIndexInfo{
				    .buffer = rtContext.GetVertexBuffer(),
				    .offset = rtContext.GetIndexOffset(),
				    .range  = rtContext.GetIndexSize(),
                };
                writeDescriptorSets[write_index++] = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				    .dstSet = rtDescriptorSet,
				    .dstBinding = BINDING_INDICES,
				    .descriptorCount = 1,
				    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				    .pBufferInfo = &descriptorIndexInfo,
                };

                // Vertex offset buffer
                VkDescriptorBufferInfo descriptorVertexLocationInfo{
				    .buffer = rtContext.GetVertexBuffer(),
				    .offset = rtContext.GetVertexLocationOffset(),
				    .range  = rtContext.GetVertexLocationSize(),
                };
                writeDescriptorSets[write_index++] = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				    .dstSet = rtDescriptorSet,
				    .dstBinding = BINDING_VERTEX_LOCATIONS,
				    .descriptorCount = 1,
				    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				    .pBufferInfo = &descriptorVertexLocationInfo,
                };

                // Index offset buffer
                VkDescriptorBufferInfo descriptorIndexLocationInfo{
				    .buffer = rtContext.GetVertexBuffer(),
				    .offset = rtContext.GetIndexLocationOffset(),
				    .range  = rtContext.GetIndexLocationSize(),
                };
                writeDescriptorSets[write_index++] = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				    .dstSet = rtDescriptorSet,
				    .dstBinding = BINDING_INDEX_LOCATIONS,
				    .descriptorCount = 1,
				    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				    .pBufferInfo = &descriptorIndexLocationInfo,
                };

                // Normal buffer
                //VkDescriptorBufferInfo descriptorNormalInfo{
				//    .buffer = rtContext.GetVertexBuffer(),
				//    .offset = rtContext.GetNormalOffset(),
				//    .range  = rtContext.GetNormalSize(),
                //};
                //writeDescriptorSets[4] = {
                //    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				//    .dstSet = rtDescriptorSet,
				//    .dstBinding = BINDING_NORMALS,
				//    .descriptorCount = 1,
				//    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				//    .pBufferInfo = &descriptorNormalInfo,
                //};
                assert(write_index == bindingNum);

				vkUpdateDescriptorSets(device,
					uint32_t(writeDescriptorSets.size()),  // Number of VkWriteDescriptorSet objects
					writeDescriptorSets.data(),            // Pointer to VkWriteDescriptorSet objects
					0, nullptr);                           // An array of VkCopyDescriptorSet objects (unused)
			}

            auto rtShaders = rtContext.LoadRayTracingShaders();

            auto rtShaderStages = RayTracingContext::ShaderStages{};
            {
                auto& stages = rtShaderStages.m_stages;
                stages.resize(3);
				// Ray generation shader stage
				stages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
				stages[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
				stages[0].module = rtShaders.aoRgen;
				stages[0].pName = "main";

				// Miss shader
				stages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
				stages[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
				stages[1].module = rtShaders.aoRmiss;
				stages[1].pName = "main";

				// Closest hit shader (used for primary ray casts)
				stages[2] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
				stages[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
				stages[2].module = rtShaders.aoCHitPrimary;
				stages[2].pName = "main";
            }


            auto rtShaderGroups = RayTracingContext::ShaderGroups{};
            {
                auto& groups = rtShaderGroups.m_groups;
                groups.resize(3);
				// Ray generation shader group
				groups[0] = { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
				groups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
				groups[0].generalShader = 0;
				groups[0].closestHitShader = VK_SHADER_UNUSED_KHR;
				groups[0].anyHitShader = VK_SHADER_UNUSED_KHR;
				groups[0].intersectionShader = VK_SHADER_UNUSED_KHR;

				// Miss shader group
				groups[1] = groups[0];
				groups[1].generalShader = 1;

				// Closest hit shader group (used for primary ray casts)
				groups[2] = { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
				groups[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
				groups[2].generalShader = VK_SHADER_UNUSED_KHR;
				groups[2].closestHitShader = 2;
				groups[2].anyHitShader = VK_SHADER_UNUSED_KHR;
				groups[2].intersectionShader = VK_SHADER_UNUSED_KHR;
            }

            auto rtPipelineCreateInfo = RayTracingContext::RaytracingPipelineCreateInfo{
                .shaders        = rtShaders,
                .pipelineLayout = rtPipelineLayout,
                .stages         = rtShaderStages,
                .groups         = rtShaderGroups,
            };
            auto rtPipeline = RayTracingContext::RayTracingPipeline(device, rtPipelineCreateInfo);

            auto sbtHeaderSize      = (DeviceSize)rtContext.GetShaderGroupHandleSize();
            auto sbtBaseAlignment   = (DeviceSize)rtContext.GetShaderGroupBaseAlignment();
			auto sbtHandleAlignment = (DeviceSize)rtContext.GetShaderGroupHandleAlignment();
			auto sbtStride          = (DeviceSize)sbtBaseAlignment * ((sbtHeaderSize + sbtBaseAlignment - 1) / sbtBaseAlignment);

			assert(sbtBaseAlignment% sbtHandleAlignment == 0);
			assert(sbtStride <= rtContext.GetMaxShaderGroupStride());

            auto rtShaderHandleStorage = std::vector<uint8_t> (sbtHeaderSize * rtShaderGroups.size());
            vkGetRayTracingShaderGroupHandlesKHR(device,
                rtPipeline,                   // Pipeline
                0,                            // First group
                rtShaderGroups.size(),        // Number of groups
                rtShaderHandleStorage.size(), // Size of buffer
                rtShaderHandleStorage.data()  // Data buffer
            ) | VK_NO_ERROR;


            auto sbtSize = uint32_t(sbtStride * rtShaderGroups.size());
            auto sbtBuffer = rtCreateInfo.allocator.CreateBufferVma(BufferVmaCreateInfo{
                .bufferCreateInfo {
                    .size = sbtSize,
                    .usage = BufferUsageFlagBits::ShaderDeviceAddress | BufferUsageFlagBits::ShaderBindingTableKhr,
                },
                .allocationCreateInfo {
                    .usage = +MemoryUsage::CpuToGpu,    
                },
            });
            {
                void* data;
                sbtBuffer.MapMemory(&data);
                auto mappedSBT = reinterpret_cast<uint8_t*>(data);
				for (auto groupIndex : iota(0u, rtShaderGroups.size())) {
					memcpy(&mappedSBT[groupIndex * sbtStride], &rtShaderHandleStorage[groupIndex * sbtHeaderSize], sbtHeaderSize);
				}
                sbtBuffer.UnmapMemory();
            }


            CommandBufferAllocateInfo rtCmdBufferAllocateInfo {
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = static_cast<uint32_t>(swapchainImageViews.size()),
            };
            auto rtCommandBuffers = raytracingCommandPool.AllocateCommandBufferArray(rtCmdBufferAllocateInfo);

            for (auto i : iota(0uz, rtCommandBuffers.size())) {
                auto cmdBuf = rtCommandBuffers[i];
                cmdBuf.BeginRecording();
				{
                    // prepare output image
                    auto subresourceRange = VkImageSubresourceRange{
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	                    .baseMipLevel = 0,
	                    .levelCount = 1,
	                    .baseArrayLayer = 0,
	                    .layerCount = 1,
                    };

                    cmdBuf.PipelineImageMemoryBarrier2KHR(std::array{
                        ImageMemoryBarrier2KHR{
                            .srcStageMask     = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,
                            .srcAccessMask    = VK_ACCESS_2_NONE_KHR,
                            .dstStageMask     = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
                            .dstAccessMask    = VK_ACCESS_2_SHADER_WRITE_BIT_KHR,
                            .oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED,
                            .newLayout        = VK_IMAGE_LAYOUT_GENERAL,
                            .image            = rtImages[i],
                            .subresourceRange = subresourceRange,
                        },
                    });

                    cmdBuf.BindPipeline(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline);
                    auto rtDescriptorSetToBind = std::array{rtDescriptorSetArray[i]};
                    cmdBuf.BindDescriptorSets(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineLayout, 0, rtDescriptorSetToBind);

                    auto sbtStartAddress = (DeviceAddress)sbtBuffer.GetDeviceAddress();

	                // 1 ray gen shader
                    auto sbtRegionRGen = VkStridedDeviceAddressRegionKHR{
	                    .deviceAddress = sbtStartAddress,
	                    .stride        = sbtStride,
	                    .size          = sbtStride,
                    };

	                // 1 miss shader
                    auto sbtRegionMiss = VkStridedDeviceAddressRegionKHR{
	                    .deviceAddress = sbtStartAddress + sbtStride,
	                    .stride        = sbtStride,
	                    .size          = sbtStride,
                    };

	                // 1 closest hit shader
                    auto sbtRegionHit = VkStridedDeviceAddressRegionKHR{
	                    .deviceAddress = sbtStartAddress + 2 * sbtStride,
	                    .stride        = sbtStride,
	                    .size          = sbtStride,
                    };

	                // 0 callable shaders
                    auto sbtRegionCallable = VkStridedDeviceAddressRegionKHR{
	                    .deviceAddress = 0,
	                    .stride        = 0,
	                    .size          = 0,
                    };

                    cmdBuf.TraceRaysKHR(sbtRegionRGen, sbtRegionMiss, sbtRegionHit, sbtRegionCallable, swapchain.extent().width, swapchain.extent().height, 1);

                    cmdBuf.PipelineImageMemoryBarrier2KHR(std::array{
                        ImageMemoryBarrier2KHR{
                            .srcStageMask     = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
                            .srcAccessMask    = VK_ACCESS_2_SHADER_WRITE_BIT_KHR,
                            .dstStageMask     = VK_PIPELINE_STAGE_2_BLIT_BIT_KHR,
                            .dstAccessMask    = VK_ACCESS_2_TRANSFER_READ_BIT_KHR,
                            .oldLayout        = VK_IMAGE_LAYOUT_GENERAL,
                            .newLayout        = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            .image            = rtImages[i],
                            .subresourceRange = subresourceRange,
                        },
                        ImageMemoryBarrier2KHR{
                            .srcStageMask     = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT_KHR,
                            .srcAccessMask    = VK_ACCESS_2_NONE_KHR,
                            .dstStageMask     = VK_PIPELINE_STAGE_2_BLIT_BIT_KHR,
                            .dstAccessMask    = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
                            .oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED,
                            .newLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            .image            = swapchain.image(i),
                            .subresourceRange = subresourceRange,
                        },
                    });
#if 1
                    auto copyRegions = std::array{
                        VkImageCopy2KHR{
                            .sType = VK_STRUCTURE_TYPE_IMAGE_COPY_2_KHR,
                            .srcSubresource = {
                                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                .mipLevel = 0,
                                .baseArrayLayer = 0,
                                .layerCount = 1
                            },
	                        .srcOffset = { 0, 0, 0 },
	                        .dstSubresource = {
                                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                .mipLevel = 0,
                                .baseArrayLayer = 0,
                                .layerCount = 1
                            },
	                        .dstOffset = { 0, 0, 0 },
	                        .extent = Extent2Dto3D(swapchain.extent()), //{ .width = swapchain.extent().width, .height = swapchain.extent().height, .depth = 1 },
                        }};
                    cmdBuf.CopyImage2KHR(CopyImageInfo2KHR{
                            .srcImage       = rtImages[i],
                            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            .dstImage       = swapchain.image(i),
                            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            .regionCount    = copyRegions.size(),
                            .pRegions       = copyRegions.data(),
                        });
#endif
#if 0
                    auto region = VkImageBlit2KHR{
                            .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2_KHR,
                            .srcSubresource = {
                                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                .mipLevel = 0,
                                .baseArrayLayer = 0,
                                .layerCount = 1
                            },
                            .srcOffsets = {
                                {0, 0, 0},
                                {(int32_t)swapchain.extent().width, (int32_t)swapchain.extent().height, 1},
                            },
	                        .dstSubresource = {
                                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                .mipLevel = 0,
                                .baseArrayLayer = 0,
                                .layerCount = 1
                            },
                            .dstOffsets = {
                                {0, 0, 0},
                                {(int32_t)swapchain.extent().width, (int32_t)swapchain.extent().height, 1},
                            },
                    };
                    cmdBuf.BlitImage2KHR(BlitImageInfo2KHR{
                            .srcImage = rtImages[i],
                            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            .dstImage = swapchain.image(i),
                            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            .regionCount = 1,
                            .pRegions = &region,
                            .filter = VK_FILTER_LINEAR,
                        });
#endif

                    cmdBuf.PipelineImageMemoryBarrier2KHR(std::array{
                        ImageMemoryBarrier2KHR{
                            .srcStageMask     = VK_PIPELINE_STAGE_2_BLIT_BIT_KHR,
                            .srcAccessMask    = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR,
                            .dstStageMask     = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT_KHR,
                            .dstAccessMask    = VK_ACCESS_2_NONE_KHR,
                            .oldLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            .newLayout        = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                            .image            = swapchain.image(i),
                            .subresourceRange = subresourceRange,
                        },
                    });
				}
                cmdBuf.EndRecording();
            }
#pragma endregion

#pragma region Synchronization
            std::vector<Semaphore> imageAvailableSemaphores;
            std::vector<Semaphore> renderFinishedSemaphores;
            std::vector<Fence> framesInFlightFences;
            for (auto i : iota(0uz, maxFramesInFlight)) {
                imageAvailableSemaphores.emplace_back(device, default_allocator);
                renderFinishedSemaphores.emplace_back(device, default_allocator);
                framesInFlightFences.emplace_back(device, FenceCreateFlagBits::Signaled, default_allocator);
            }
            std::vector<Fence const*> imagesInFlightFences(swapchainImageViews.size(), nullptr);
            Fence pushConstantFence(device, FenceCreateFlagBits::Unsignaled);
#pragma endregion Synchronization

#pragma region Mainloop
#ifdef RASTERIZATION_ON
            meshUploadingContext.fence.Wait();
#endif

            while (!window.ShouldClose()) {
				time = glfw::GetTime();
                glfw::PollEvents();

#   pragma region Physics
#   pragma endregion Physics


#   pragma region Rendering
                framesInFlightFences[currentFrame].Wait();

                // rendering -- acquire
                std::array imageIndices = {~0u};
                auto& imageIndex = imageIndices[0];
				{
                    AcquireNextImageInfoKHR acquireInfo{
                        .semaphore = imageAvailableSemaphores[currentFrame],
                    };
                    auto result = swapchain.AcquireNextImage2KHR(acquireInfo, OUT imageIndex);
					if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                        device.WaitIdle();
						goto recreate_swapchain;
					}
					else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
						throw std::runtime_error("Vulkan error: failed to acquire swapchain image!");
					}
				}

#pragma region rasterization
#ifdef RASTERIZATION_ON
                // update uniform buffer
                auto current_time = static_cast<float>(glfw::GetTime());
                auto aspect = static_cast<float>(swapchain.extent().width) / swapchain.extent().height;
                UniformBufferObject ubo{
                    .time = static_cast<glm::float32>(current_time),
                    .resolution = glm::vec2(window.getWidth(), window.getHeight()),
                    .model = enableRotation ? glm::rotate(glm::mat4(1.0f), current_time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)) : glm::mat4(1.0f),
                    .view  = glm::lookAt(lookAtEye, lookAtCenter, glm::vec3(0.0f, 0.0f, 1.0f)),
                    .proj  = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10.0f),
                };
                ubo.proj[1][1] *= -1; // OpenGL to Vulkan
                uniformBuffers[imageIndex].LoadData(&ubo, sizeof(ubo));

                //      Check if a previous frame is using this image (i.e. there is its fence to wait on)
                auto& imageFence = imagesInFlightFences[imageIndex];
                if (imageFence != nullptr) {
                    imageFence->Wait();
                }
                //      Mark the image as now being in use by this frame
                imageFence = &framesInFlightFences[currentFrame];

                // rendering -- submit

                //auto pushConstantCmdBuf = pushConstantCommandBuffers[0];
                //pushConstantCmdBuf.BeginRecording();
                //{
                //    auto pushConstantRange = pushConstantRanges[0];
                //    pushConstant.time = glfw::GetTime();
                //    LogInfo("{}\n", pushConstant.time);
                //    pushConstantCmdBuf.PushConstants(graphicsPipeline.getLayout(), pushConstantRange, &pushConstant);
                //}
                //pushConstantCmdBuf.EndRecording();
                //VkSubmitInfo pushConstantSubmitInfo{
                //    .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                //    .waitSemaphoreCount   = 0,
                //    .pWaitSemaphores      = nullptr,
                //    .pWaitDstStageMask    = waitStages.data(),
                //    .commandBufferCount   = (uint32_t) pushConstantCommandBuffers.size(),
                //    .pCommandBuffers      = pushConstantCommandBuffers.data(),
                //    .signalSemaphoreCount = 0,
                //    .pSignalSemaphores    = nullptr,
                //};
                //pushConstantFence.Reset();
                //graphicsQueue.Submit(1, &pushConstantSubmitInfo, pushConstantFence);
                //pushConstantFence.Wait();
                std::array<VkSemaphore         , 1> waitSemaphores     = {imageAvailableSemaphores[currentFrame]};
                std::array<VkPipelineStageFlags, 1> waitStages         = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
                std::array<VkSemaphore         , 1> signalSemaphores   = {renderFinishedSemaphores[currentFrame]};

                VkSubmitInfo submitInfo{
                    .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                    .waitSemaphoreCount   = waitSemaphores.size(),
                    .pWaitSemaphores      = waitSemaphores.data(),
                    .pWaitDstStageMask    = waitStages.data(),
                    .signalSemaphoreCount = signalSemaphores.size(),
                    .pSignalSemaphores    = signalSemaphores.data(),
                };

                //std::array indices{ imageIndex, pushConstIndex };
                framesInFlightFences[currentFrame].Reset();
                commandBuffers.SubmitToQueue(graphicsQueue, imageIndex, submitInfo, framesInFlightFences[currentFrame]);
#endif
#pragma endregion rasterization

#pragma region raytracing
                // update uniform buffer
                auto current_time = static_cast<float>(glfw::GetTime());
                auto aspect = static_cast<float>(swapchain.extent().width) / swapchain.extent().height;
                RaytracingUBO ubo{
                    .time = static_cast<glm::float32>(current_time),
                    .cameraTransform = rtCameraTransform,
                    .cameraRayTransform = rtCameraRayTransform,
                };
                rtUniformBuffers[imageIndex].LoadData(&ubo, sizeof(ubo));

                //      Check if a previous frame is using this image (i.e. there is its fence to wait on)
                auto& imageFence = imagesInFlightFences[imageIndex];
                if (imageFence != nullptr) {
                    imageFence->Wait();
                }
                //      Mark the image as now being in use by this frame
                imageFence = &framesInFlightFences[currentFrame];
                std::array<VkSemaphore         , 1> waitSemaphores     = {imageAvailableSemaphores[currentFrame]};
                std::array<VkPipelineStageFlags, 1> waitStages         = {VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR};
                std::array<VkSemaphore         , 1> signalSemaphores   = {renderFinishedSemaphores[currentFrame]};

                VkSubmitInfo submitInfo{
                    .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                    .waitSemaphoreCount   = waitSemaphores.size(),
                    .pWaitSemaphores      = waitSemaphores.data(),
                    .pWaitDstStageMask    = waitStages.data(),
                    .signalSemaphoreCount = signalSemaphores.size(),
                    .pSignalSemaphores    = signalSemaphores.data(),
                };

                //std::array indices{ imageIndex, pushConstIndex };
                framesInFlightFences[currentFrame].Reset();
                rtCommandBuffers.SubmitToQueue(raytracingQueue, imageIndex, submitInfo, framesInFlightFences[currentFrame]);
#pragma endregion raytracing

                // rendering -- present
                std::array<VkSwapchainKHR, 1> swapchains = {swapchain};
                VkPresentInfoKHR presentInfo{
                    .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                    .waitSemaphoreCount = signalSemaphores.size(),
                    .pWaitSemaphores    = signalSemaphores.data(),
                    .swapchainCount     = swapchains.size(),
                    .pSwapchains        = swapchains.data(),
                    .pImageIndices      = imageIndices.data(),
                    .pResults           = nullptr, // Optional
                };

				{
					auto result = vkQueuePresentKHR(device.m_queuePresent, &presentInfo);

					if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
                        device.WaitIdle();
                        goto recreate_swapchain;
					}
					else if (result != VK_SUCCESS) {
						throw std::runtime_error("Vulkan error: failed to present swapchain image!");
					}
				}

                currentFrame = (currentFrame + 1) % maxFramesInFlight;
#   pragma endregion Rendering

                double new_time = glfw::GetTime();
				window.SetTitle(new_time - time);
            }
#pragma endregion Mainloop

            device.WaitIdle();
		}
        glm::vec3 rotate(in_t<glm::vec3> dir, in_t<glm::vec3> axis, float angle) {
            auto v4 = glm::rotate(glm::mat4(1.0f), angle, topDir) * glm::vec4(dir, 1.0f);
            return { v4[0], v4[1], v4[2] };
        }
        void rtUpdateAccordingTo(int key) {
            //LogInfo("Unimplemented key hit: {}\n", static_cast<char>(key));
            // TODO: update cameraTransform
            auto rotate_angle = glm::pi<float>() * 0.05f * steplength;
            switch (key) {
            case glfw::key::Num[1]:
                steplength -= 0.05f;
                if (steplength < 0.0f) steplength = 0.01f;
                break;
            case glfw::key::Num[2]:
                steplength = 0.10f;
                break;
            case glfw::key::Num[3]:
                steplength += 0.05f;
                break;
            case glfw::key::W:
                rtCameraTransform = glm::translate(rtCameraTransform, steplength * faceDir);
                break;
            case glfw::key::A:
                rtCameraTransform = glm::translate(rtCameraTransform, steplength * rotate(faceDir, topDir, glm::half_pi<float>()));
                break;
            case glfw::key::S:
                rtCameraTransform = glm::translate(rtCameraTransform, -steplength * faceDir);
                break;
            case glfw::key::D:
                rtCameraTransform = glm::translate(rtCameraTransform, -steplength * rotate(faceDir, topDir, glm::half_pi<float>()));
                break;
            case glfw::key::Q:
                rtCameraRayTransform = glm::rotate(rtCameraRayTransform, rotate_angle, topDir);
                faceDir = rotate(faceDir, topDir, rotate_angle);
                break;
            case glfw::key::E:
                rtCameraRayTransform = glm::rotate(rtCameraRayTransform, -rotate_angle, topDir);
                faceDir = rotate(faceDir, topDir, -rotate_angle);
                break;
            default:
                LogInfo("Unimplemented key hit: {}\n", static_cast<char>(key));
            }
        }

        void updateAccordingTo(int key) {
            //LogInfo("Unimplemented key hit: {}\n", static_cast<char>(key));
            // TODO: update cameraTransform
#ifdef RASTERIZATION_ON
            switch (key) {
            case glfw::key::Num[1]:
                enableRotation = !enableRotation;
                break;
            case glfw::key::Num[2]:
                lookAtCenter.z += 0.1f;
                break;
            case glfw::key::Num[3]:
                lookAtCenter.z -= 0.1f;
                break;
            case glfw::key::W:
                lookAtEye    += -glm::vec3(0.1f, 0.1f, 0.0f);
                lookAtCenter += -glm::vec3(0.1f, 0.1f, 0.0f);
                break;
            case glfw::key::A:
                lookAtEye    += glm::vec3(0.1f, -0.1f, 0.0f);
                lookAtCenter += glm::vec3(0.1f, -0.1f, 0.0f);
                break;
            case glfw::key::S:
                lookAtEye    += glm::vec3(0.1f, 0.1f, 0.0f);
                lookAtCenter += glm::vec3(0.1f, 0.1f, 0.0f);
                break;
            case glfw::key::D:
                lookAtEye    += glm::vec3(-0.1f, 0.1f, 0.0f);
                lookAtCenter += glm::vec3(-0.1f, 0.1f, 0.0f);
                break;
            default:
                LogInfo("Unimplemented key hit: {}\n", static_cast<char>(key));
            }
#endif
        }
	};

	// 
	// 
	void appKeyCallback(glfw::PWindow window, int key, int scancode, int action, int mods) {
        Application* app_ptr{ nullptr };
		if (action == glfw::Press || action == glfw::Repeat) {
			switch (key) {
			case glfw::key::Escape:
				glfw::SetWindowShouldClose(window, glfw::True);
				break;
            case glfw::key::W:
            case glfw::key::A:
            case glfw::key::S:
            case glfw::key::D:
            case glfw::key::Q:
            case glfw::key::E:
            case glfw::key::Num[1]:
            case glfw::key::Num[2]:
            case glfw::key::Num[3]:
                app_ptr = static_cast<Application*>(glfw::GetWindowUserPointer(window));
                app_ptr->rtUpdateAccordingTo(key);
                break;
			default:
				LogWarning("Unknown key hit: {}\n", static_cast<char>(key));
			}
		}
	}
    //
    // #Entry_point_and_app_info
    //
	void TestVulkan() {
        InitVolkAndCheckVulkanVersion(/*minimum_major: */1, /*minimum_minor: */ 2);


		// all in one create info
        RayTracingPipelineKHR rayTracingPipelineFeatureKHR;
        AccelerationStructureFeaturesKHR accelerationStructureFeatureKHR;
        MeshShaderFeaturesNV meshShaderFeatureNV;
        Synchronization2FeaturesKHR synchronization2FeatureKHR;
        //BufferDeviceAddressFeatures bufferDeviceAddressFeature;
        auto validationFeaturesToEnable = std::array{ VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT };
        auto validationInfo = VkValidationFeaturesEXT{
            .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
            .enabledValidationFeatureCount = validationFeaturesToEnable.size(),
            .pEnabledValidationFeatures    = validationFeaturesToEnable.data(),
        };
#ifdef _WIN32
        _putenv_s("DEBUG_PRINTF_TO_STDOUT", "1");
#else
        putenv("DEBUG_PRINTF_TO_STDOUT=1");
#endif // _WIN32
		ApplicationCreateInfo applicationCreateInfo{
			.instanceCreateInfo {
				.appName	= "Vulkan Raytracing Renderer",
				.engineName	= "UntitledEngine",
				.apiMajor	= 1,
				.apiMinor	= 2,
				.requiredLayerNames = {
					//"VK_LAYER_LUNARG_api_dump",
					//"VK_LAYER_KHRONOS_validation", // enabled by ApplicationCreateInfo::enableValidationLayer()
				},
				.requiredExtensionNames = {
				// the first two are required by glfw
					VK_KHR_SURFACE_EXTENSION_NAME,		 //vk::KhrSurfaceExtensionName,
					VK_KHR_WIN32_SURFACE_EXTENSION_NAME,  //vk::KhrWin32SurfaceExtensionName
				},
                .pRequiredFeatures = &validationInfo,
			},
			.deviceCreateInfo {
				.requiredLayerNames {
				},
				.requiredExtensionNames {
                // copy buffer:
                    VK_KHR_COPY_COMMANDS_2_EXTENSION_NAME,
                // mesh shader:
                    VK_NV_MESH_SHADER_EXTENSION_NAME,
				// sync:
					VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
				// window presentation
					VK_KHR_SWAPCHAIN_EXTENSION_NAME,
				// ray tracing:
					VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
					VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
					//VK_KHR_RAY_QUERY_EXTENSION_NAME,
                    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
                // shader debug printf:
                    VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
				},
			    .requiredFeatures {
                    &rayTracingPipelineFeatureKHR,
                    &accelerationStructureFeatureKHR,
                    &meshShaderFeatureNV,
                    &synchronization2FeatureKHR,
                },
                .optionalFeatures {
                },
				.checkers {
				}
			},
			.windowCreateInfo {
				.width   = 800,
				.height  = 600,
				.title   = "Mage",
				.monitor = nullptr,
				.share	 = nullptr,
                .keyCallback = appKeyCallback,
			},
			.shaderSources {
				"shader.vert",
				"shader.frag",
			}
		};
		applicationCreateInfo.enableValidationLayer();

		Application application(applicationCreateInfo);
	}


} // export namespace vk

//
// #Details
//

module :private;
namespace vk {

    void InitVolkAndCheckVulkanVersion(uint32_t minimum_major, uint32_t minimum_minor) {
        volkInitialize() | VK_NO_ERROR;
        //if (vkGetInstanceProcAddr(null_handle, "vkEnumerateInstanceVersion") == nullptr) {
        //    throw std::runtime_error("Error: Incompatible Driver!");
        //} else {
            uint32_t apiVersion;
            vkEnumerateInstanceVersion(&apiVersion) | VK_NO_ERROR;
            auto major = ApiVersionMajor(apiVersion);
            auto minor = ApiVersionMinor(apiVersion);
            if (not (major == minimum_major && minor >= minimum_minor)) {
                throw std::runtime_error("Error: Incompatible Driver!");
            }
            LogWarning("Instance API Version: {}.{}\n", major, minor);
        //}
    }

    VkResult LayerProperty::getExtensionProperties(std::vector<VkPhysicalDevice> gpus) {
        auto extensionCount = uint32_t{};
        auto result         = VkResult{};
        auto layerName      = property.layerName;

        do {
            if (not gpus.empty()) result = vkEnumerateDeviceExtensionProperties(gpus[0], layerName, OUT & extensionCount, null_handle);
            else				  result = vkEnumerateInstanceExtensionProperties(layerName, OUT & extensionCount, null_handle);

            if (result != VK_SUCCESS || extensionCount == 0) continue;

            extensions.resize(extensionCount);
            if (not gpus.empty()) result = vkEnumerateDeviceExtensionProperties(gpus[0], layerName, IN & extensionCount, extensions.data());
            else				  result = vkEnumerateInstanceExtensionProperties(layerName, IN & extensionCount, extensions.data());
        } while (result == VK_INCOMPLETE);

        return result;
    }

    VkResult LayerAndExtension::getInstanceLayerProperties() {
        auto count           = uint32_t{};
        auto layerProperties = std::vector<VkLayerProperties>{};
        auto result          = VkResult{};

        do {
            result = vkEnumerateInstanceLayerProperties(OUT & count, null_handle);
            if (result != VK_SUCCESS) {
                return result;
            }
            if (count == 0) {
                throw std::runtime_error("No available instance layer!");
            }
            layerProperties.resize(count);
            result = vkEnumerateInstanceLayerProperties(IN & count, layerProperties.data());
        } while (result == VK_INCOMPLETE);

        LogInfo("Instance Layers:\n");
        LogInfo("----------------\n");
        for (const auto& globalLayerProperty : layerProperties) {
            bool enabled = false;

            for (auto requiredLayerName : requiredLayerNames) {
                if (strcmp(requiredLayerName, globalLayerProperty.layerName) == 0) {
                    enabled = true;
                    enabledLayerNames.push_back(requiredLayerName);
                    break;
                }
            }
            LogLE(enabled, "{}\n", globalLayerProperty.description);
            LogLE(enabled, "{:4}|\n", ' ');
            LogLE(enabled, "{:4}|---[Layer Name]--> {}\n", ' ', globalLayerProperty.layerName);

            LayerProperty layerProperty{
                    .property = globalLayerProperty
            };
            result = layerProperty.getExtensionProperties();
            if (result != VK_SUCCESS) {
                continue;
            }

            for (const auto& extension : layerProperty.extensions) {
                enabled = false;
                for (auto requiredExtensionName : requiredExtensionNames) {
                    if (strcmp(requiredExtensionName, extension.extensionName) == 0) {
                        enabled = true;
                        //enabledExtensionNames.push_back(requiredExtensionName);
                        break;
                    }
                }
                LogLE(enabled, "{:8}|\n", ' ');
                LogLE(enabled, "{:8}|---[Layer Extension]--> {}\n", ' ', extension.extensionName);
            }
            layerPropertyList.push_back(std::move(layerProperty));
        }
        LogInfo("\n");
        return result;
    }

    VkResult LayerAndExtension::getInstanceExtensionProperties() {
        auto extensions = std::vector<VkExtensionProperties>{};
        auto extensionCount = uint32_t{};
        auto result = VkResult{};
        do {
            result = vkEnumerateInstanceExtensionProperties(nullptr, OUT & extensionCount, null_handle);

            if (result != VK_SUCCESS || extensionCount == 0) continue;

            extensions.resize(extensionCount);
            result = vkEnumerateInstanceExtensionProperties(nullptr, IN & extensionCount, extensions.data());
        } while (result == VK_INCOMPLETE);

        LogInfo("Instance extensions:\n");
        LogInfo("--------------------\n");
        for (const auto& extension : extensions) {
            bool enabled = false;
            for (auto requiredExtensionName : requiredExtensionNames) {
                if (strcmp(requiredExtensionName, extension.extensionName) == 0) {
                    enabled = true;
                    enabledExtensionNames.push_back(requiredExtensionName);
                    break;
                }
            }
            LogLE(enabled, "{:4}|\n", ' ');
            LogLE(enabled, "{:4}|---[Instance Extension]--> {}\n", ' ', extension.extensionName);
        }
        LogInfo("\n");
        return result;
    }

    void LayerAndExtension::initEnabledInstanceLayerAndExtensions() {
        //if (requiredLayerNames.size() != enabledLayerNames.size())
        LogWarning("Unsupported layers:\n");
        for (auto requiredLayerName : requiredLayerNames) {
            bool enabled = false;
            for (auto enabledLayerName : enabledLayerNames) {
                if (strcmp(requiredLayerName, enabledLayerName) == 0) {
                    enabled = true;
                    break;
                }
            }
            if (not enabled) {
                LogWarning("{:4}|\n", ' ');
                LogWarning("{:4}|---[Layer Name]--> {}\n", ' ', requiredLayerName);
            }
            // if not enabled and not optional
            // LogError
            // throw std::runtime_error
        }

        //if (requiredExtensionNames.size() != enabledExtensionNames.size())
        LogWarning("Unsupported layer extensions:\n");
        for (auto requiredExtensionName : requiredExtensionNames) {
            bool enabled = false;
            for (auto enabledExtensionName : enabledExtensionNames) {
                if (strcmp(requiredExtensionName, enabledExtensionName) == 0) {
                    enabled = true;
                    break;
                }
            }
            if (not enabled) {
                LogWarning("{:4}|\n", ' ');
                LogWarning("{:4}|---[Layer Extension]--> {}\n", ' ', requiredExtensionName);
            }
            // if not enabled and not optional
            // LogError
            // throw std::runtime_error
        }
        LogWarning("\n");
    }


    VkResult LayerAndExtension::getDeviceExtensionProperties(const LayerAndExtension& instanceLayerExtension, GpuList gpus) {
        VkResult result;

        LogInfo("Device extensions by layers:\n");
        LogInfo("----------------------------\n");
        const auto& instanceLayerProperties = instanceLayerExtension.layerPropertyList;
        //instanceLayerProperties.push_back(LayerProperty{.property = VkLayerProperties{.layerName = nullptr} });
        for (const auto& globalLayerProperty : instanceLayerProperties) {
            LayerProperty layerProperty{
                    .property = globalLayerProperty.property
            };
            result = layerProperty.getExtensionProperties(gpus);
            if (result != VK_SUCCESS) {
                continue;
            }
            LogInfo("{}\n", globalLayerProperty.property.description);
            LogInfo("{:4}|\n", ' ');
            LogInfo("{:4}|---[Layer Name]--> {}\n", ' ', globalLayerProperty.property.layerName);

            if (not layerProperty.extensions.empty()) {
                for (const auto& extension : layerProperty.extensions) {
                    bool enabled = false;
                    for (auto requiredExtensionName : requiredExtensionNames) {
                        if (strcmp(requiredExtensionName, extension.extensionName) == 0) {
                            enabled = true;
                            //enabledExtensionNames.push_back(requiredExtensionName);
                            break;
                        }
                    }
                    LogLE(enabled, "{:8}|\n", ' ');
                    LogLE(enabled, "{:8}|--[Device Extension]--> {}\n", ' ', extension.extensionName);
                }
            }
            else {
                LogInfo("{:8}|\n", ' ');
                LogInfo("{:8}|---[Device Extension]--> No extension found \n", ' ');
            }
            layerPropertyList.push_back(std::move(layerProperty));
        }
        LogInfo("\n");
        // device extensions
        // initEnabledDeviceExtensions();
        return result;
    }

    bool LayerAndExtension::initEnabledDeviceExtensions(VkPhysicalDevice gpu) {
        auto extensionCount = uint32_t{};
        auto extensions     = std::vector<VkExtensionProperties>{};
        auto result         = VkResult{};
        enabledExtensionNames.clear();

        do {
            result = vkEnumerateDeviceExtensionProperties(gpu, nullptr, OUT & extensionCount, null_handle);

            if (result != VK_SUCCESS || extensionCount == 0) continue;

            extensions.resize(extensionCount);
            result = vkEnumerateDeviceExtensionProperties(gpu, nullptr, IN & extensionCount, extensions.data());
        } while (result == VK_INCOMPLETE);

        LogInfo("Device Extensions:\n");
        LogInfo("------------------\n");
        for (const auto& extension : extensions) {
            bool enabled = false;
            for (auto requiredExtensionName : requiredExtensionNames) {
                if (strcmp(requiredExtensionName, extension.extensionName) == 0) {
                    enabled = true;
                    enabledExtensionNames.push_back(requiredExtensionName);
                    break;
                }
            }
            LogLE(enabled, "{:4}|\n", ' ');
            LogLE(enabled, "{:4}|---[Device Extension]--> {}\n", ' ', extension.extensionName);
        }
        LogInfo("\n");
        // TODO: optional device extensions
        if (enabledExtensionNames.size() == requiredExtensionNames.size()) {
            return true;
        }
        sort(enabledExtensionNames.begin(), enabledExtensionNames.end());
        sort(requiredExtensionNames.begin(), requiredExtensionNames.end());
        for (auto i = enabledExtensionNames.size(); i < requiredExtensionNames.size(); ++i) {
            LogWarning("Extension {} is required but not supported by current device!\n", requiredExtensionNames[i]);
        }
        return false;
    }

    Instance::Instance(const InstanceCreateInfo& info) :
            layerExtensions(info.requiredLayerNames, info.requiredExtensionNames),
            validationEnabled(info.validationEnabled) {
        // check if layers are supported
        checkInstanceLayersAndExtensions();
        // create instance
        VkApplicationInfo appInfo{
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pNext = end_of_chain,
                .pApplicationName = info.appName,
                //.applicationVersion = 1,
                .pEngineName = info.engineName,
                //.engineVersion = 1,
                .apiVersion = MakeApiVersion(0, info.apiMajor, info.apiMinor, 0)
        };
        VkInstanceCreateInfo createInfo{
                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .pNext = info.pRequiredFeatures,
                .flags = 0,
                .pApplicationInfo = &appInfo,
                .enabledLayerCount = static_cast<uint32_t>(layerExtensions.enabledLayerNames.size()),
                .ppEnabledLayerNames = layerExtensions.enabledLayerNames.data(),
                .enabledExtensionCount = static_cast<uint32_t>(layerExtensions.enabledExtensionNames.size()),
                .ppEnabledExtensionNames = layerExtensions.enabledExtensionNames.data()
        };
        vkCreateInstance(IN & createInfo, default_allocator, OUT & m_instance) | VK_NO_ERROR;
        volkLoadInstanceOnly(m_instance);
        ApiVersion = appInfo.apiVersion;

#if !__has_include(<volk.h>)
        loadExtensionFunctions();
#endif
        if (validationEnabled) {
            initDebugUtils();
        }
    }

    void Instance::initDebugUtils() {
        VkDebugUtilsMessengerCreateInfoEXT createInfo{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .pNext = end_of_chain,
                .flags = 0,
                .messageSeverity = //VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                               | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                               | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                .pfnUserCallback = debugMessengerCallback,
                .pUserData = nullptr
        };
#if !__has_include(<volk.h>)
        m_vkCreateDebugUtilsMessengerEXT(m_instance, IN & createInfo, default_allocator, OUT & m_debugMessenger);
#else
        vkCreateDebugUtilsMessengerEXT(m_instance, IN & createInfo, default_allocator, OUT & m_debugMessenger);
#endif
    }

    PhysicalDevice::PhysicalDevice(Instance& m_instance, LayerAndExtension& layerExtensions, DeviceCreateInfo& info) : gpuCheckerList(info.checkers) {
        // physical devices and properties
        uint32_t gpuCount;
        vkEnumeratePhysicalDevices(m_instance, OUT & gpuCount, null_handle)   | VK_NO_ERROR;
        gpuList.resize(gpuCount);
        vkEnumeratePhysicalDevices(m_instance, IN & gpuCount, gpuList.data()) | VK_NO_ERROR;

        // check required device layers and extensions
        // gpuCheckerList[i].result = whether gpuList[i] is available
        gpuCheckerList.push_back(Checker{
                .check = [&layerExtensions](VkPhysicalDevice gpu) {
                    return layerExtensions.initEnabledDeviceExtensions(gpu);
                },
                .info = "Some required device extensions are unavailable."
        });
        for (auto& checker : gpuCheckerList | reverse) {
            for (auto gpu : gpuList) {
                if (checker.check(gpu)) {
                    checker.result.push_back(VK_TRUE);
                } else {
                    checker.result.push_back(VK_FALSE);
                }
            }
        }

        m_gpu = null_handle;
        for (auto i = 0uz; i < gpuList.size(); ++i) {
            LogInfo("Checking GPU {}\n", i);
            bool support = true;
            for (const auto& checker : gpuCheckerList) {
                support = support && checker.result[i];
            }
            if (support) {
                m_gpu = gpuList[i];
                break;
            }
        }
        if (m_gpu == null_handle) {
            throw std::runtime_error("Critical Error: No GPU Supports All Reqruied Extensions"); // TODO: and features
        }

        for (auto extension : info.requiredExtensionNames) {
            if (strcmp(extension, VK_NV_MESH_SHADER_EXTENSION_NAME)) {
                gpuProperties12.pNext = &meshShaderProperties;
            }
        }
        vkGetPhysicalDeviceProperties2(m_gpu, &gpuProperties);
        vkGetPhysicalDeviceMemoryProperties2(m_gpu, &gpuMemoryProperties);

        // TODO: check required features in checker
        gpuFeatures12.pNext = info.requiredFeatures[0]->ptr();
        for (auto i = 0uz; i < info.requiredFeatures.size()-1; ++i) {
            info.requiredFeatures[i]->pNext() = info.requiredFeatures[i+1]->ptr();
        }
        vkGetPhysicalDeviceFeatures2(m_gpu, &gpuFeatures);
        bool allFeaturesEnabled = true;
        for (auto pFeature : info.requiredFeatures) {
            if (not pFeature->enabled()) {
                allFeaturesEnabled = false;
                LogError("Unavailable required feature: {}\n", pFeature->name());
            }
        }
        if (not allFeaturesEnabled) {
            throw std::runtime_error("Some required features are unavailable. For details please enable the api_dump layer.");
        }

        // queue family properties
        uint32_t queueFamilyCount;
        vkGetPhysicalDeviceQueueFamilyProperties(m_gpu, OUT & queueFamilyCount, null_handle);
        queueFamilyPropsList.resize(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_gpu, IN & queueFamilyCount, OUT queueFamilyPropsList.data());
    }

    Queue Device::takeQueue(QueueFlags flags) {
        for (uint32_t qi = 0; qi < unusedQueues.size(); ++qi) {
            const auto& queueInfo = unusedQueues[qi];
            if ((gpu.queueFamilyPropsList[queueInfo.familyIndex].queueFlags & flags) == flags) {
                Queue queue{
                        .m_device = this,
                        .queueIndex = queueInfo.queueIndex,
                        .familyIndex = queueInfo.familyIndex
                };
                vkGetDeviceQueue(m_device, queue.familyIndex, queue.queueIndex, &queue.m_queue);
                unusedQueues.erase(unusedQueues.begin() + qi);
                return queue;
            }
        }
        return {.m_device = this};
    }
    Device::Device(DeviceCreateInfo& info, Instance& instance_, const VkAllocationCallbacks* allocator) :
            m_instance(instance_),
            m_allocator(allocator),
            layerExtensions(info.requiredLayerNames, info.requiredExtensionNames),
            gpu(m_instance, layerExtensions, info),
            m_queueGCT     {.m_device = this},
            m_queueCT      {.m_device = this},
            m_queueT       {.m_device = this},
            m_queuePresent {.m_device = this}
    {
        layerExtensions.getDeviceExtensionProperties(m_instance.getLayerExtensions(), gpu.getGpuList());
        //layerExtensions.initEnabledDeviceExtensions(gpu.getGpuList());

        //
        // a general purpose queue family (graphics, compute, transfer) is required
        // priorities are not set and sharing a common uniform priority list {1.0, 1.0, ...}
        // one queueCreateInfo for each queue family
        //
        std::vector<float> priorities;
        bool generalQueueFamily = false;
        for (const auto& qfProperty : gpu.queueFamilyPropsList) {
            if (qfProperty.queueFlags & QFlagGCT == QFlagGCT) {
                generalQueueFamily = true;
            }
            if (qfProperty.queueCount > priorities.size()) {
                priorities.resize(qfProperty.queueCount, 1.f);
            }
        }
        if (not generalQueueFamily) {
            throw std::runtime_error("Error: no general purpose queue family!");
        }
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        for (uint32_t i = 0; i < gpu.queueFamilyPropsList.size(); ++i) {
            queueCreateInfos.emplace_back(VkDeviceQueueCreateInfo{
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .pNext = end_of_chain,
                    .queueFamilyIndex = i,
                    .queueCount = gpu.queueFamilyPropsList[i].queueCount,
                    .pQueuePriorities = priorities.data()
            });
        }

        //
        // setup extensions and features, and create device
        //

        NameList usedDeviceExtensions;
        usedDeviceExtensions = layerExtensions.enabledExtensionNames;
        VkDeviceCreateInfo deviceCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .pNext = gpu.features(), // gpu.additionalFeatures(), //gpu.features(),
                //.flags = 0, reserved for future use
                .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
                .pQueueCreateInfos = queueCreateInfos.data(),
                //.enabledLayerCount = 0,
                //.ppEnabledLayerNames = nullptr,
                .enabledExtensionCount = static_cast<uint32_t>(usedDeviceExtensions.size()),
                .ppEnabledExtensionNames = usedDeviceExtensions.data(),
                .pEnabledFeatures = nullptr // features are enabled by pNext chain
        };
        vkCreateDevice(gpu, IN & deviceCreateInfo, m_allocator, OUT & m_device) | VK_NO_ERROR;
        volkLoadDevice(m_device);
#if !__has_include(<volk.h>)
        loadExtensionFunctions();
#endif
#ifdef VOLK_DEVICE_TABLE_ENABLED
        volkLoadDeviceTable(&table, m_device);
#endif

        // scan available queues
        for (uint32_t familyIndex = 0; familyIndex < gpu.queueFamilyPropsList.size(); ++familyIndex) {
            const auto& queueFamily = gpu.queueFamilyPropsList[familyIndex];
            QueueInfo queueInfo{ .score = 0, .familyIndex = familyIndex };
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                ++queueInfo.score;
            }
            if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
                ++queueInfo.score;
            }
            if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
                ++queueInfo.score;
            }
            for (uint32_t queueIndex = 0; queueIndex < queueFamily.queueCount; ++queueIndex) {
                queueInfo.queueIndex = queueIndex;
                unusedQueues.push_back(queueInfo);
            }
        }

        std::sort(unusedQueues.begin(), unusedQueues.end());
        m_queueGCT = takeQueue(QFlagGCT); isValidQueue(m_queueGCT);
        m_queueCT  = takeQueue(QFlagC);   //isValidQueue(m_queueCT);
        m_queueT   = takeQueue(QFlagT);   //isValidQueue(m_queueT);

        // default presentation queue is set to be the general queue, but it will be checked later when initialize vk::Swapchain
        m_queuePresent = m_queueGCT;
        //LogInfo("GCT queue: {}, {}\n", m_queueGCT.familyIndex, m_queueGCT.queueIndex);
        //LogInfo("Cmp queue: {}, {}\n", m_queueCT.familyIndex, m_queueCT.queueIndex);
        //LogInfo("Tsf queue: {}, {}\n", m_queueT.familyIndex, m_queueT.queueIndex);
    }

    void Device::checkSurfaceSupportAndSetPresentQueue(VkSurfaceKHR surface, bool raytracing) {
        LogInfo("Checking surface support...\n");
        std::vector<uint32_t> queueFamiliesSupportPresentation;
        VkBool32 surfaceSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(gpu, m_queueGCT.familyIndex, surface, &surfaceSupport) | VK_NO_ERROR;
        if (surfaceSupport) {
            LogInfo("---- General queue (GCT) support presentation.\n");
            queueFamiliesSupportPresentation.push_back(m_queueGCT.familyIndex);
        }
        vkGetPhysicalDeviceSurfaceSupportKHR(gpu, m_queueCT.familyIndex, surface, &surfaceSupport) | VK_NO_ERROR;
        if (surfaceSupport) {
            LogInfo("---- Compute queue (C) support presentation.\n");
            queueFamiliesSupportPresentation.push_back(m_queueCT.familyIndex);
        } else {
            LogWarning("Renderer is running in the ray tracing mode while the ray tracing queue doesn't support presentation\n");
        }
        vkGetPhysicalDeviceSurfaceSupportKHR(gpu, m_queueT.familyIndex, surface, &surfaceSupport) | VK_NO_ERROR;
        if (surfaceSupport) {
            LogInfo("---- Transfer queue (T) support presentation.\n");
            queueFamiliesSupportPresentation.push_back(m_queueT.familyIndex);
        }

        if (queueFamiliesSupportPresentation.empty()) {
            throw std::runtime_error("Vulkan error: no queue family support presentation!");
        }

        if (raytracing) {
            m_queuePresent = m_queueCT;
            LogInfo("---- Present queue is set to be the raytracing (compute) queue.\n");
        } else if (auto presentQF = queueFamiliesSupportPresentation[0]; presentQF != m_queueGCT.familyIndex) {
            bool transferQ = presentQF == m_queueT.familyIndex;
            m_queuePresent = transferQ ? m_queueT : m_queueCT;
            LogWarning("---- General queue doesn't support presentation and {} queue is used instead!\n", transferQ ? "transfer" : "compute");
        }
        //vkGetPhysicalDeviceSurfaceSupportKHR(gpu, m_queuePresent.familyIndex, surface, &surfaceSupport);
        //if (surfaceSupport) {
        //	LogInfo("Present queue (GCT) support presentation.\n");
        //}
    }


    Swapchain::Swapchain(Device& device_, Surface& surface_, bool raytracing) : m_device(device_), m_surface(surface_), m_allocator(m_device.allocator()) {
        // flush the temporary value
        // TODO: see the TODO for fillSwapchainSupportDetails
        auto& swapchainSupport = m_surface.fillSwapchainSupportDetails(m_device);
        auto  surfaceFormat	   = m_surface.chooseSwapchainSurfaceFormat();
        auto  presentMode	   = m_surface.chooseSwapchainPresentMode();
        auto  extent		   = m_surface.chooseSwapchainExtent();

        uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
        // assert(imageCount == 3u);

        if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount) {
            imageCount = swapchainSupport.capabilities.maxImageCount;
        }

        m_device.checkSurfaceSupportAndSetPresentQueue(m_surface, raytracing);
        auto queueFamilyIndices = std::array{
            raytracing ? m_device.m_queueCT.familyIndex : m_device.m_queueGCT.familyIndex,
            m_device.m_queuePresent.familyIndex
        };
        bool singleQueue = queueFamilyIndices[0] == queueFamilyIndices[1];

        VkSwapchainCreateInfoKHR createInfo{
                .sType   = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                //.pNext = end_of_chain,
                //.flags = 0,
                .surface = m_surface,

                .minImageCount    = imageCount,
                .imageFormat      = surfaceFormat.format,
                .imageColorSpace  = surfaceFormat.colorSpace,
                .imageExtent      = extent,
                .imageArrayLayers = 1,
                .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,

                .imageSharingMode      = singleQueue ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
                .queueFamilyIndexCount = singleQueue ? 1u : 2u,
                .pQueueFamilyIndices   = queueFamilyIndices.data(),

                .preTransform   = swapchainSupport.capabilities.currentTransform,
                .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                .presentMode    = presentMode,
                .clipped        = VK_TRUE,
                .oldSwapchain   = VK_NULL_HANDLE,
        };
        if (raytracing) {
            createInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            LogInfo("The swapchain is solely for raytracing\n");
        }

        vkCreateSwapchainKHR(m_device, &createInfo, m_allocator, &m_swapchain) | VK_NO_ERROR;

        vkGetSwapchainImagesKHR(m_device, m_swapchain, OUT & imageCount, nullptr) | VK_NO_ERROR;
        swapchainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(m_device, m_swapchain, IN & imageCount, OUT swapchainImages.data()) | VK_NO_ERROR;

        swapchainImageFormat = surfaceFormat.format;
        swapchainExtent      = extent;
    }

	// input:
	//		optional:
	//			VkPhysicalDevice
		// TODO: add a read-only version
		//SwapchainSupportDetails getSupportDetails(VkPhysicalDevice gpu) const { SwapchainSupportDetails supportDetails; return supportDetails; }
    SwapchainSupportDetails& Surface::fillSwapchainSupportDetails(VkPhysicalDevice gpu) {

    	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, m_surface, &supportDetails.capabilities) | VK_NO_ERROR;

    	uint32_t formatCount;
    	vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, m_surface, OUT &formatCount, nullptr) | VK_NO_ERROR;

    	if (formatCount != 0) {
    		supportDetails.formats.resize(formatCount);
    		vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, m_surface, IN &formatCount, OUT supportDetails.formats.data()) | VK_NO_ERROR;
    	}

    	uint32_t presentModeCount;
    	vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, m_surface, OUT &presentModeCount, nullptr) | VK_NO_ERROR;

    	if (presentModeCount != 0) {
    		supportDetails.presentModes.resize(presentModeCount);
    		vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, m_surface, &presentModeCount, supportDetails.presentModes.data()) | VK_NO_ERROR;
    	}

    	return supportDetails;
    }

    VkSurfaceFormatKHR Surface::chooseSwapchainSurfaceFormat() {
		const auto& availableFormats = supportDetails.formats;
		for (const auto& availableFormat : availableFormats) {
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				LogInfo("[Format {}, Colorspace {}] is selected.\n",
					VkEnumToName(availableFormat.format), VkEnumToName(availableFormat.colorSpace));
				return availableFormat;
			}
		}
		auto& defaultFormat = availableFormats[0];
		LogWarning("SRGB format is not available and a default [Format {}, Colorspace {}] is selected.\n",
			VkEnumToName(defaultFormat.format), VkEnumToName(defaultFormat.colorSpace));
		return defaultFormat;
	}

    VkPresentModeKHR Surface::chooseSwapchainPresentMode() {
        const auto& availablePresentModes = supportDetails.presentModes;
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                LogInfo("PresentMode {} is selected.\n", VkEnumToName(availablePresentMode));
                return availablePresentMode;
            }
        }
        auto defaultPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        LogWarning("PresentMode MAILBOX is not available and a default {} is selected instead.\n",
                   VkEnumToName(defaultPresentMode));
        return defaultPresentMode;
    }

    VkExtent2D Surface::chooseSwapchainExtent() {
        const auto& capabilities = supportDetails.capabilities;
        if (capabilities.currentExtent.width != UINT32_MAX) {
            LogInfo("Window extent is [width {} x height {}]\n", capabilities.currentExtent.width, capabilities.currentExtent.height);
            return capabilities.currentExtent;
        }
        else {
            VkExtent2D actualExtent{
                    .width  = std::clamp(static_cast<uint32_t>(width) , capabilities.minImageExtent.width , capabilities.maxImageExtent.width ),
                    .height = std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
            };
            LogWarning("Surface capabilities don't contain enough information\
							and window extent is read from glfw [width {} x height {}]\n",
                       capabilities.currentExtent.width, capabilities.currentExtent.height);
            return actualExtent;
        }
    }

    std::vector<ImageView> Swapchain::getImageViews() {
        std::vector<ImageView> swapchainImageViews;
        for (auto image : swapchainImages) {
            ImageViewCreateInfo imageViewCreateInfo{
                .image    = image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format   = swapchainImageFormat,
                .components = VkComponentMapping {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY
                },
                .subresourceRange = VkImageSubresourceRange{
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                }
            };
            swapchainImageViews.emplace_back(m_device, imageViewCreateInfo, m_allocator);
        }
        return swapchainImageViews;
    }

    GraphicsPipeline::GraphicsPipeline(Device const& device, PipelineCreateInfo& info, const VkAllocationCallbacks* allocator) :
        m_device(device),
        m_pipelineLayout(device, info.pipelineLayoutInfo, allocator),
        m_renderPass(device, info.renderPassInfo, allocator),
        m_allocator(allocator)
    {
        //
        // programmable stages of the graphic pipeline
        //
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

        ShaderModule vertexShaderModule(device, info.spirv.vertexShaderCode, allocator);
        VkPipelineShaderStageCreateInfo vertexShaderStateInfo {
            .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage               = VK_SHADER_STAGE_VERTEX_BIT,
            .module              = vertexShaderModule,
            .pName               = "main", // entry point
            .pSpecializationInfo = nullptr,
        };
        shaderStages.push_back(vertexShaderStateInfo);

        ShaderModule fragmentShaderModule(device, info.spirv.fragmentShaderCode, allocator);
        VkPipelineShaderStageCreateInfo fragmentShaderStateInfo {
            .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module              = fragmentShaderModule,
            .pName               = "main", // entry point
            .pSpecializationInfo = nullptr,
        };
        shaderStages.push_back(fragmentShaderStateInfo);

        std::unordered_map<std::string, ShaderModule> shaderModules;
        for (auto& [shadername, shadercode] : info.spirv.shaderCodes) {
            shaderModules.emplace(std::piecewise_construct,
                std::forward_as_tuple(shadername),
                std::forward_as_tuple(device, shadercode, allocator));
            VkPipelineShaderStageCreateInfo meshShaderStateInfo{
                .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage               = VK_SHADER_STAGE_MESH_BIT_NV,
                .module              = shaderModules.at(shadername),
                .pName               = "main",
                .pSpecializationInfo = nullptr,
            };
        }

        //
        // fixed stages of the graphic pipeline
        //

        VkPipelineVertexInputStateCreateInfo vertexInput{
            .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount   = static_cast<uint32_t>(info.vertexInputBindingDescriptions.size()),
            .pVertexBindingDescriptions      = info.vertexInputBindingDescriptions.data(),
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(info.vertexInputAttributeDescriptions.size()),
            .pVertexAttributeDescriptions    = info.vertexInputAttributeDescriptions.data(),
        };

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        };

        const auto swapchainImageExtent = info.swapchain.extent();
        VkViewport viewport{
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<float>(swapchainImageExtent.width),
            .height   = static_cast<float>(swapchainImageExtent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        VkRect2D scissor{
            .offset {
                .x = 0,
                .y = 0,
            },
            .extent = swapchainImageExtent,
        };
        VkPipelineViewportStateCreateInfo viewportState{
            .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .pViewports    = &viewport,
            .scissorCount  = 1,
            .pScissors     = &scissor,
        };

        VkPipelineRasterizationStateCreateInfo rasterizerState{
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable        = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode             = VK_POLYGON_MODE_FILL,
            .cullMode                = VK_CULL_MODE_BACK_BIT,
            .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable         = VK_FALSE,
            .depthBiasConstantFactor = 0.0f, // Optional
            .depthBiasClamp          = 0.0f, // Optional
            .depthBiasSlopeFactor    = 0.0f, // Optional
            .lineWidth               = 1.0f,
        };

        VkPipelineMultisampleStateCreateInfo multisamplingState{
            .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable   = VK_FALSE,
            .minSampleShading      = 1.0f, // Optional
            .pSampleMask           = nullptr, // Optional
            .alphaToCoverageEnable = VK_FALSE, // Optional
            .alphaToOneEnable      = VK_FALSE, // Optional
        };

        VkPipelineColorBlendAttachmentState colorBlendAttachmentState_alphaBlending{
                .blendEnable         = VK_TRUE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                .colorBlendOp        = VK_BLEND_OP_ADD,
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                .alphaBlendOp        = VK_BLEND_OP_ADD,
                .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };
        VkPipelineColorBlendAttachmentState colorBlendAttachmentState{
            .blendEnable         = VK_FALSE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_ONE, // Optional
            .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO, // Optional
            .colorBlendOp        = VK_BLEND_OP_ADD, // Optional
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE, // Optional
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO, // Optional
            .alphaBlendOp        = VK_BLEND_OP_ADD, // Optional
            .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };
        VkPipelineColorBlendStateCreateInfo colorBlendState{
            .sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable     = VK_FALSE,
            .logicOp           = VK_LOGIC_OP_COPY, // Optional
            .attachmentCount   = 1,
            .pAttachments      = &colorBlendAttachmentState,
            .blendConstants	   = { 0.0f, 0.0f, 0.0f, 0.0f },// Optional
        };

        const std::array dynamicStatesArray{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_LINE_WIDTH
        };
        VkPipelineDynamicStateCreateInfo dynamicState{
            .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = dynamicStatesArray.size(),
            .pDynamicStates    = dynamicStatesArray.data(),
        };
       
        //
        // create the graphics pipeline
        //
        VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{
            .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount          = static_cast<uint32_t>(shaderStages.size()),
            .pStages             = shaderStages.data(),
            .pVertexInputState   = &vertexInput,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState      = &viewportState,
            .pRasterizationState = &rasterizerState,
            .pMultisampleState   = &multisamplingState,
            .pDepthStencilState  = nullptr,
            .pColorBlendState    = &colorBlendState,
            .pDynamicState       = nullptr,
            .layout              = m_pipelineLayout,
            .renderPass          = m_renderPass,
            .subpass             = 0,
            .basePipelineHandle  = VK_NULL_HANDLE, // Optional
            .basePipelineIndex   = -1, // Optional
        };

        vkCreateGraphicsPipelines(m_device, null_handle, 1, &graphicsPipelineCreateInfo, allocator, &m_pipeline) | VK_NO_ERROR;
    }

    Framebuffer ImageView::createFramebuffer(const Swapchain& swapchain, VkRenderPass renderPass) const {
        FramebufferCreateInfo createInfo{
                .renderPass      = renderPass,
                .attachmentCount = 1,
                .pAttachments    = &m_imageView,
                .width           = swapchain.extent().width,
                .height          = swapchain.extent().height,
                .layers          = 1,
        };
        return Framebuffer(m_device, createInfo, m_allocator);
    }

    void CommandBuffer::ExecuteCommandBuffers(in_t<CommandBufferArray> cmdBuffers) {
        vkCmdExecuteCommands(m_buffer, cmdBuffers.size(), cmdBuffers.data());
    }

    CommandBufferArray::~CommandBufferArray() {
		if (not m_commandBuffers.empty()) {
			vkFreeCommandBuffers(m_device, m_pool, static_cast<uint32_t>(m_commandBuffers.size()), m_commandBuffers.data());
		}
    }

    [[nodiscard]]
    BufferVma ResourceAllocator::CreateBufferVma(in_t<BufferVmaCreateInfo> info) {
        VkBuffer buffer;
        VmaAllocation allocation;
        vmaCreateBuffer(m_vmaAllocator, reinterpret_cast<const VkBufferCreateInfo*>(&info.bufferCreateInfo), &info.allocationCreateInfo, &buffer, &allocation, nullptr) | VK_NO_ERROR;
        return BufferVma(buffer, allocation, *this);
    }

    [[nodiscard]]
    ImageVma ResourceAllocator::CreateImageVma(in_t<ImageVmaCreateInfo> info) {
        VkImage image;
        VmaAllocation allocation;
        vmaCreateImage(m_vmaAllocator, reinterpret_cast<const VkImageCreateInfo*>(&info.imageCreateInfo), &info.allocationCreateInfo, &image, &allocation, nullptr) | VK_NO_ERROR;
        return ImageVma(image, allocation, *this);
    }

	void StagingBuffer::LoadTexture(in_t<StbImage> stbImage) {
		m_size = stbImage.size();
		if (m_capacity < m_size) {
			ReallocateBuffer(m_size);
		}
		m_buffer.LoadData(stbImage.data(), stbImage.size());
	}
}
