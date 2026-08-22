cmake_minimum_required(VERSION 3.70)

project(main)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

# packages & programs
find_package(glfw3 3.3 REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_program(IWYU_PATH NAMES include-what-you-use iwyu)
if (NOT IWYU_PATH)
    message(FATAL_ERROR "Could not find include-what-you-use")
endif()

set(SOURCE 
    src/main.cpp
    src/App/App.cpp 
    src/Window/Window.cpp 
    src/Input/Input.cpp 
    src/Renderer/Renderer.cpp
)

set(LIBS
    libs/volk.c
    libs/vmaImplementation.cpp
)

add_executable(${PROJECT_NAME} ${SOURCE} ${LIBS})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

# treat src & lib as a root
target_include_directories(${PROJECT_NAME} PRIVATE src)
target_include_directories(${PROJECT_NAME} PRIVATE libs)

# defines
target_compile_definitions(${PROJECT_NAME} PRIVATE
    GLFW_INCLUDE_VULKAN
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_DYNAMIC_VULKAN_FUNCTIONS=0
)

# link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    GPUOpen::VulkanMemoryAllocator
)

# use programs
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_INCLUDE_WHAT_YOU_USE ${IWYU_PATH})
