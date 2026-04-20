# before cmake configure, first run vcpkg install commands
# git clone https://github.com/microsoft/vcpkg
# .\vcpkg\bootstrap-vcpkg.bat
# .\vcpkg\vcpkg install openssl zlib protobuf sdl3[vulkan] lz4 simdjson
# if using cmake-gui add CMAKE configuration entry: CMAKE_TOOLCHAIN_FILE <MANUALLY SPECIFY PATH>/vcpkg/scripts/buildsystems/vcpkg.cmake

cmake_minimum_required(VERSION 3.21)

project(CLFDTD
	LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CONFIGURATION_TYPES
    Debug
    Release
    CACHE STRING "" FORCE
)

set(VULKAN_SDK_VERSION "1.4.335"
    CACHE STRING "Vulkan SDK version to target (used for volk, headers, etc.)"
)
set(VULKAN_SDK_TAG "vulkan-sdk-${VULKAN_SDK_VERSION}"
    CACHE STRING "Vulkan SDK git tag"
)

find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)

add_subdirectory(external)

# source code

file(GLOB_RECURSE 
    CLFDTD_SOURCES
        src/*.c
        src/*.cpp
        src/*.h
)
add_executable(CLFDTD ${CLFDTD_SOURCES})

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTY VS_STARTUP_PROJECT CLFDTD
)

target_include_directories(CLFDTD
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

foreach(_file IN LISTS CLFDTD_SOURCES)
    file(RELATIVE_PATH _rel_file "${CMAKE_CURRENT_SOURCE_DIR}/src" "${_file}")
    get_filename_component(_dir "${_rel_file}" PATH)
    source_group("${_dir}" FILES "${_file}")
endforeach()

# libraries

target_link_libraries(CLFDTD
    PRIVATE
	volk
	Vulkan::shaderc_combined
        Vulkan::Headers
        fmt
        SDL3::SDL3
	vk-bootstrap
	dearimgui
)

# output directories

set(ASSETS_DIR ${CMAKE_SOURCE_DIR}/assets)
set(CLFDTD_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/out)

file (MAKE_DIRECTORY ${CLFDTD_OUTPUT_DIRECTORY})

# platform configuration

if (WIN32)
    target_compile_options(CLFDTD PRIVATE
        /MP
        $<$<CONFIG:Debug>:/Od /fp:fast /Zi>
        $<$<CONFIG:Release>:/O2 /fp:fast /Ob2 /Oi /Ot /Qpar /GL /DNDEBUG>
    )

    target_link_options(CLFDTD PRIVATE
        $<$<CONFIG:Debug>:/DEBUG>
        $<$<CONFIG:Release>:/LTCG>
    )

    set_target_properties(CLFDTD PROPERTIES
        WIN32_EXECUTABLE FALSE

        RUNTIME_OUTPUT_DIRECTORY ${CLFDTD_OUTPUT_DIRECTORY}/$<CONFIG>/runtime
        LIBRARY_OUTPUT_DIRECTORY ${CLFDTD_OUTPUT_DIRECTORY}/$<CONFIG>/lib
        ARCHIVE_OUTPUT_DIRECTORY ${CLFDTD_OUTPUT_DIRECTORY}/$<CONFIG>/lib
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:CLFDTD>"
    )

    target_compile_definitions(CLFDTD
        PRIVATE
            NOMINMAX # fix windows max() macro bug
            VK_NO_PROTOTYPES
            GLM_ENABLE_EXPERIMENTAL
    )

endif()
