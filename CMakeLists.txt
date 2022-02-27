cmake_minimum_required(VERSION 3.21)
project(Mage
	VERSION 0.1.1
	LANGUAGES CXX)

include(CMake/CPM.cmake)

CPMAddPackage(
        NAME GLFW
        GITHUB_REPOSITORY glfw/glfw
        GIT_TAG 3.3.6
        OPTIONS
            "GLFW_BUILD_TESTS OFF"
            "GLFW_BUILD_EXAMPLES OFF"
            "GLFW_BULID_DOCS OFF"
)

CPMAddPackage(
        NAME Eigen
        VERSION 3.4.0
        URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
        # Eigen's CMakeLists are for installation instead of library use; for example it will create an "uninstall" target, conflicting with the one introduced by GLFW
        DOWNLOAD_ONLY YES
)
if(Eigen_ADDED)
    add_library(Eigen INTERFACE IMPORTED)
    target_include_directories(Eigen INTERFACE ${Eigen_SOURCE_DIR})
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(Eigen INTERFACE OpenMP::OpenMP_CXX)
    endif()
endif()

CPMAddPackage(
        NAME fmt
        OPTIONS "CMAKE_CXX_STANDARD 20" #"FMT_MODULE ON"
        GIT_TAG 8.1.1
        GITHUB_REPOSITORY fmtlib/fmt
)

CPMAddPackage(
        NAME ImGui
        VERSION 1.87
        URL https://github.com/ocornut/imgui/archive/refs/tags/v1.85.tar.gz
        DOWNLOAD_ONLY YES )
if (ImGui_ADDED)
    message(STATUS ${ImGui_SOURCE_DIR})
    add_library(ImGui STATIC)
    target_sources(ImGui PUBLIC
        ${ImGui_SOURCE_DIR}/imconfig.h # (empty by default, user-editable)
        ${ImGui_SOURCE_DIR}/imgui.cpp
        ${ImGui_SOURCE_DIR}/imgui.h
        ${ImGui_SOURCE_DIR}/imgui_demo.cpp
        ${ImGui_SOURCE_DIR}/imgui_draw.cpp
        ${ImGui_SOURCE_DIR}/imgui_internal.h
        ${ImGui_SOURCE_DIR}/imgui_tables.cpp
        ${ImGui_SOURCE_DIR}/imgui_widgets.cpp
        ${ImGui_SOURCE_DIR}/imstb_rectpack.h
        ${ImGui_SOURCE_DIR}/imstb_textedit.h
        ${ImGui_SOURCE_DIR}/imstb_truetype.h
    )
    target_include_directories(ImGui PUBLIC ${ImGui_SOURCE_DIR})
endif()

CPMAddPackage(
        NAME stb
        GIT_TAG af1a5bc352164740c1cc1354942b1c6b72eacb8a
        GITHUB_REPOSITORY nothings/stb
        DOWNLOAD_ONLY YES
)
if (stb_ADDED)
    add_library(stb INTERFACE IMPORTED)
    target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
endif()

CPMAddPackage(
        NAME glm
        GITHUB_REPOSITORY g-truc/glm
        GIT_TAG 0.9.9.8
)

CPMAddPackage(
        NAME VulkanMemoryAllocator
        GITHUB_REPOSITORY GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
        #GIT_TAG e821342782fdb15a4bc54b9ccd7dfe68b19bae7f
        GIT_TAG 4039e45a196c100adc49e0c66085a5c7b86420b0
)

CPMAddPackage(
        NAME volk
        GITHUB_REPOSITORY zeux/volk
        GIT_TAG 1.3.204
)

#find_package(Vulkan 1.2.189.1 REQUIRED)

add_library(TinyObjLoader INTERFACE IMPORTED)
target_include_directories(TinyObjLoader INTERFACE Includes)

add_executable(Mage)
	target_sources(Mage PRIVATE
		    Sources/Mage.cpp
            Sources/Physics.cppm
            Sources/Physics-Types.cppm
		    Sources/Application.cppm
		    #Sources/App.ixx
            Sources/Window.cppm
            Sources/Vulkan.cppm
            #Sources/Vulkan.cpp
		    Sources/Vulkan-Error.cppm
            #Sources/Vulkan-Macros.ixx
            #Sources/Renderer.ixx
	)
	target_link_libraries(Mage PRIVATE
	        Eigen
	        glfw
	        #Vulkan::Vulkan
            ImGui
	        fmt::fmt
            glm::glm
            VulkanMemoryAllocator
            volk::volk_headers
            TinyObjLoader
            stb
    )
	target_compile_features(Mage PUBLIC cxx_std_23)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Mage)

    cmake_host_system_information(RESULT PHYSICAL_CORES QUERY NUMBER_OF_PHYSICAL_CORES)
    message(STATUS "CMake found ${PHYSICAL_CORES} physical cores")
    add_compile_definitions(Mage PRIVATE
        PHYSICAL_CORES=${PHYSICAL_CORES}
        ASSET_PATH="${CMAKE_SOURCE_DIR}/Assets"
        SHADER_SOURCE_PATH="${CMAKE_SOURCE_DIR}/Assets/Shaders"
        MODEL_PATH="${CMAKE_SOURCE_DIR}/Assets/Models"
        TEXTURE_PATH="${CMAKE_SOURCE_DIR}/Assets/Textures"
    )
#end(Mage)

# Compile shaders to SPIR-V

file(GLOB_RECURSE GLSL_SOURCE_FILES #Comments are taken from glslangValidator --help
    "Assets/Shaders/*.conf"         #to provide a config file that replaces the default configuration
                                    #(see -c option below for generating a template)
    "Assets/Shaders/*.vert"         #for a vertex shader
    "Assets/Shaders/*.tesc"         #for a tessellation control shader
    "Assets/Shaders/*.tese"         #for a tessellation evaluation shader
    "Assets/Shaders/*.geom"         #for a geometry shader
    "Assets/Shaders/*.frag"         #for a fragment shader
    "Assets/Shaders/*.comp"         #for a compute shader
    "Assets/Shaders/*.mesh"         #for a mesh shader
    "Assets/Shaders/*.task"         #for a task shader
    "Assets/Shaders/*.rgen"         #for a ray generation shader
    "Assets/Shaders/*.rint"         #for a ray intersection shader
    "Assets/Shaders/*.rahit"        #for a ray any hit shader
    "Assets/Shaders/*.rchit"        #for a ray closest hit shader
    "Assets/Shaders/*.rmiss"        #for a ray miss shader
    "Assets/Shaders/*.rcall"        #for a ray callable shader
    "Assets/Shaders/*.glsl"         #for .vert.glsl, .tesc.glsl, ..., .comp.glsl compound suffixes
    "Assets/Shaders/*.hlsl"         #for .vert.hlsl, .tesc.hlsl, ..., .comp.hlsl compound suffixes
)

foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_BINARY_DIR}/Shaders/${FILE_NAME}.spv")
  message(STATUS "Shader to compile: ${GLSL}")
  add_custom_command(
    OUTPUT ${SPIRV}
    #COMMAND ${CMAKE_COMMAND} -E echo "Compiling shaders: " "${GLSL}" # Visual Studio prints a similar message
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/Shaders/"
    COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} --target-env vulkan1.2 ${GLSL} -g -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(Mage Shaders)

add_custom_command(TARGET Mage POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Mage>/Shaders/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_BINARY_DIR}/Shaders"
        "$<TARGET_FILE_DIR:Mage>/Shaders"
    #COMMAND ${CMAKE_COMMAND} -E remove_directory "${PROJECT_BINARY_DIR}/Shaders"
    #COMMAND ${CMAKE_COMMAND} -E echo "Target File Dir: "
    #COMMAND ${CMAKE_COMMAND} -E echo $<TARGET_FILE_DIR:Mage>
)
