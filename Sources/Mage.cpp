/*
*/
#include <fmt/format.h>
#include <stdexcept>
import Application;
import Vulkan;
import Physics;

int main()
{
	try {
        vk::TestVulkan();
    }
    catch (std::runtime_error& error) {
        vk::LogError("{}\n", error.what());
        return -1;
    }
    return EXIT_SUCCESS;
}
