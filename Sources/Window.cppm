// 
// C++20 Wrapper Module for GLFW
// Currently only work with Vulkan
//
module;
#pragma warning(push)
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#define VK_NO_PROTOTYPES
//#define VK_USE_PLATFORM_WIN32_KHR
#include "GLFW/glfw3.h"

#include <fmt/format.h>
#include <fmt/color.h>

//#include <string>
#pragma warning(pop)
#include <stdexcept>
#include <vector>
export module Window;


#define IN  /* input  variable */ 
#define OUT /* output variable */ 

// 
// Colorful log, copied from Vulkan.ixx
// 
namespace glfw {
	template <typename S, typename... Args>
	auto LogInfo(const S& format_str, const Args&... args) -> void {
		fmt::print(stdout, fmt::fg(fmt::color::sky_blue), format_str, args...);
	}

	template <typename S, typename... Args>
	auto LogWarning(const S& format_str, const Args&... args) -> void {
		fmt::print(stdout, fmt::fg(fmt::color::orange), format_str, args...);
	}

	template <typename S, typename... Args>
	auto LogError(const S& format_str, const Args&... args) -> void {
		fmt::print(stdout, fmt::fg(fmt::color::red), format_str, args...);
	}
}

#define DELETE_ALL_DEFAULT(T) \
	T() = delete; \
	T(const T&) = delete; \
	T& operator=(const T&) = delete

export namespace glfw {
	constexpr auto True = GLFW_TRUE;
	constexpr auto False = GLFW_FALSE;
	constexpr auto Press = GLFW_PRESS;
	constexpr auto Repeat = GLFW_REPEAT;
	constexpr auto Release = GLFW_RELEASE;
	namespace key {
		constexpr auto Escape = GLFW_KEY_ESCAPE;
		constexpr auto A = GLFW_KEY_A;
		constexpr auto B = GLFW_KEY_B;
		constexpr auto C = GLFW_KEY_C;
		constexpr auto D = GLFW_KEY_D;
		constexpr auto E = GLFW_KEY_E;
		constexpr auto F = GLFW_KEY_F;
		constexpr auto G = GLFW_KEY_G;
		constexpr auto H = GLFW_KEY_H;
		constexpr auto I = GLFW_KEY_I;
		constexpr auto J = GLFW_KEY_J;
		constexpr auto K = GLFW_KEY_K;
		constexpr auto L = GLFW_KEY_L;
		constexpr auto M = GLFW_KEY_M;
		constexpr auto N = GLFW_KEY_N;
		constexpr auto O = GLFW_KEY_O;
		constexpr auto P = GLFW_KEY_P;
		constexpr auto Q = GLFW_KEY_Q;
		constexpr auto R = GLFW_KEY_R;
		constexpr auto S = GLFW_KEY_S;
		constexpr auto T = GLFW_KEY_T;
		constexpr auto U = GLFW_KEY_U;
		constexpr auto V = GLFW_KEY_V;
		constexpr auto W = GLFW_KEY_W;
		constexpr auto X = GLFW_KEY_X;
		constexpr auto Y = GLFW_KEY_Y;
		constexpr auto Z = GLFW_KEY_Z;
		constexpr int Num[] = {
			GLFW_KEY_0,
			GLFW_KEY_1,
			GLFW_KEY_2,
			GLFW_KEY_3,
			GLFW_KEY_4,
			GLFW_KEY_5,
			GLFW_KEY_6,
			GLFW_KEY_7,
			GLFW_KEY_8,
			GLFW_KEY_9
		};
	}
	// default error callback
	auto error_callback(int error, const char* description) -> void;

	[[nodiscard]]
	auto GetTime() -> double {
		return glfwGetTime();
	}

	[[nodiscard]]
	auto GetTimeWithErrorCheck() {
		auto time = glfwGetTime();
		if (time == 0) {
			throw std::runtime_error("GLFW error: failed to get glfw time!");
		}
		return time;
	}


	// 
	// Window
	// 
	using PWindow = GLFWwindow*;
	using KeyCallbackFP = GLFWkeyfun; // void (*)(GLFWwindow*, int, int, int, int);
	auto SetWindowShouldClose(PWindow window, int value) -> void {
		glfwSetWindowShouldClose(window, value);
	}
	auto defaultKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) -> void {
		if (action == GLFW_PRESS) {
			switch (key) {
				case GLFW_KEY_ESCAPE:
					glfwSetWindowShouldClose(window, GLFW_TRUE);
					break;
				default:
					LogWarning("Unknown key hit: {}\n", static_cast<char>(key));
			}
		}
	}
	auto defaultMouseCallback(GLFWwindow* window, int key, int scancode, int action, int mods) -> void {
	}
#if 0
	auto joystickName = std::string{};
	auto defaultJoystickCallback(int jid, int event) -> void {
		if (event == GLFW_CONNECTED) {
			// The joystick was connected
			joystickName = glfwGetJoystickName(jid);
		}
		else if (event == GLFW_DISCONNECTED) {
			// The joystick was disconnected
			joystickName = "Disconnected";
		}
	}
#endif

	struct WindowCreateInfo {
		int width{800};
		int height{600};
		const char* title{"Untitled Window"};
		GLFWmonitor* monitor{nullptr};
		GLFWwindow* share{nullptr};
		KeyCallbackFP keyCallback{defaultKeyCallback};
	};

	class Window {
		GLFWwindow* m_window;
		std::string title_root;
		int width;
		int height;
	public:
		DELETE_ALL_DEFAULT(Window);
		Window(const WindowCreateInfo& create_info, OUT int* pFrameBufferWidth, OUT int* pFrameBufferHeight) :
			width	  {create_info.width },
			height	  {create_info.height},
			title_root{create_info.title }
		{
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			m_window = glfwCreateWindow(create_info.width, create_info.height, create_info.title, create_info.monitor, create_info.share);
			if (!m_window) {
				throw std::runtime_error("GLFW error: failed to create a glfw window!");
			}
			glfwSetKeyCallback(m_window, create_info.keyCallback);
			//glfwSetJoystickCallback(defaultJoystickCallback);
			//joystickName = glfwGetJoystickName(GLFW_JOYSTICK_1);
			glfwGetFramebufferSize(m_window, pFrameBufferWidth, pFrameBufferHeight);
		}
		~Window() {
			glfwDestroyWindow(m_window);
		}

		auto getWidth() const -> float {
			return static_cast<float>(width);
		}
		auto getHeight() const -> float {
			return static_cast<float>(height);
		}

		[[nodiscard]]
		auto ShouldClose() const -> bool {
			return glfwWindowShouldClose(m_window);
		}

		auto UpdateWindowSize() -> void {
			//glfwGetWindowSize(m_window, &width, &height);
			glfwGetFramebufferSize(m_window, &width, &height);
		}

		auto WaitEvents() const -> void {
			glfwWaitEvents();
		}

		auto isMinimized() const -> bool {
			return width == 0 || height == 0;
		}

		auto SetTitle(double time_diff) -> void {
			double time_diff_ms = time_diff * 1000;
			double fps = 1 / time_diff;
			//glfwGetWindowSize(m_window, &width, &height);
			//const auto new_title = fmt::format("{} | {} x {} | FPS: {:10.4f} | {:10.4f} ms / frame | Joystick: {}", title_root, width, height, fps, time_diff_ms, joystickName);
			const auto new_title = fmt::format("{} | {} x {} | FPS: {:10.4f} | {:10.4f} ms / frame", title_root, width, height, fps, time_diff_ms);
			glfwSetWindowTitle(m_window, new_title.c_str());
		}

		auto CreateSurface(VkInstance instance, const VkAllocationCallbacks* allocator = nullptr) -> void* {
			static_assert(sizeof(VkSurfaceKHR) == sizeof(void*));

			auto surface = VkSurfaceKHR{};
			auto vk_result = glfwCreateWindowSurface(instance, m_window, allocator, OUT &surface);
			if (vk_result != VK_SUCCESS) {
				throw std::runtime_error("GLFW error: failed to create a window surface!");
			}
			return static_cast<void*>(surface);
		}

		[[nodiscard]]
		auto GetWindowUserPointer() const -> void* {
			return glfwGetWindowUserPointer(m_window);
		}

		auto SetWindowUserPointer(void* ptr) -> void {
			glfwSetWindowUserPointer(m_window, ptr);
		}
	};

	[[nodiscard]]
	auto GetWindowUserPointer(GLFWwindow* window) -> void* {
		return glfwGetWindowUserPointer(window);
	}

    auto PollEvents() -> void {
        glfwPollEvents();
    }


	class GlfwVulkanContext {
	public:
		GlfwVulkanContext() {
			glfwSetErrorCallback(error_callback);
			auto init_result = glfwInit();
			if (init_result != GLFW_TRUE) {
				throw std::runtime_error("GLFW error: failed to initialize GLFW!");
			}
			if (not VulkanSupported()) {
				throw std::runtime_error("GLFW error: Vulkan is not supported!");
			}
		}
		~GlfwVulkanContext() {
			glfwTerminate();
		}
		auto getRequiredVkInstanceExtensions() -> std::vector<const char*> {
			uint32_t count;
			auto extensions = glfwGetRequiredInstanceExtensions(OUT &count);
			std::vector<const char*> extensionList(count);
			for (auto i = 0u; i < count; ++i) {
				extensionList[i] = extensions[i];
			}
			return extensionList;
		}
	private:
		auto VulkanSupported() -> bool {
			return glfwVulkanSupported();
		}
	};

}


module :private;
namespace glfw {
	auto error_callback(int error, const char* description) -> void {
		LogError("GLFW error: {}\n", description);
	}
}