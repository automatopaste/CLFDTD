find_package(Vulkan REQUIRED)

include(FetchContent)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.2
)

FetchContent_MakeAvailable(SDL3)

FetchContent_Declare(
    dearimgui_src
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG master
)

FetchContent_GetProperties(dearimgui_src)
if(NOT dearimgui_src_POPULATED)
    FetchContent_Populate(dearimgui_src)
endif()

add_library(dearimgui STATIC
    ${dearimgui_src_SOURCE_DIR}/imgui.cpp
    ${dearimgui_src_SOURCE_DIR}/imgui_draw.cpp
    ${dearimgui_src_SOURCE_DIR}/imgui_tables.cpp
    ${dearimgui_src_SOURCE_DIR}/imgui_widgets.cpp
    ${dearimgui_src_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    ${dearimgui_src_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
)

target_include_directories(dearimgui
    PUBLIC
        ${dearimgui_src_SOURCE_DIR}
        ${dearimgui_src_SOURCE_DIR}/backends
)

target_link_libraries(dearimgui
    PUBLIC 
        Vulkan::Vulkan
        SDL3::SDL3
)

target_compile_definitions(dearimgui
    PUBLIC
        VK_NO_PROTOTYPES
        IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
)