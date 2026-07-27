include(FetchContent)

FetchContent_Declare(
    vk-bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG ${VKBOOTSTRAP_TAG}
)

FetchContent_MakeAvailable(vk-bootstrap)

target_compile_definitions(vk-bootstrap
    PRIVATE
        VKB_VK_NO_PROTOTYPES
)