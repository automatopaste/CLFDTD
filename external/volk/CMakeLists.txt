include(FetchContent)

FetchContent_Declare(
    volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG ${VULKAN_SDK_TAG}
)

FetchContent_MakeAvailable(volk)

if (WIN32)
    target_compile_definitions(volk
        PUBLIC
            VK_USE_PLATFORM_WIN32_KHR
    )
endif()