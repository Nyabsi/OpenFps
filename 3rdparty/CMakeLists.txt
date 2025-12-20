# Extra CMake bindings for 3rdparty libraries that don't have them.

include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

set(SDL_ROOT ${CMAKE_SOURCE_DIR}/3rdparty/SDL)
set(ImGui_ROOT ${CMAKE_SOURCE_DIR}/3rdparty/ImGui)
set(ImPlot_ROOT ${CMAKE_SOURCE_DIR}/3rdparty/ImPlot)

find_package(Vulkan REQUIRED)

# Make sure SDL is built static, not shared.
set(SDL_STATIC ON)
set(SDL_SHARED OFF)

add_subdirectory(${SDL_ROOT} EXCLUDE_FROM_ALL)

add_library(vulkan INTERFACE)

target_link_libraries(vulkan INTERFACE ${Vulkan_LIBRARIES})
target_include_directories(vulkan INTERFACE ${Vulkan_INCLUDE_DIRS})

add_library(ImGui STATIC
    ${ImGui_ROOT}/backends/imgui_impl_vulkan.cpp
    ${ImGui_ROOT}/backends/imgui_impl_sdl3.cpp
    ${ImGui_ROOT}/imgui.cpp
	${ImGui_ROOT}/imgui_demo.cpp
	${ImGui_ROOT}/imgui_draw.cpp
	${ImGui_ROOT}/imgui_tables.cpp
	${ImGui_ROOT}/imgui_widgets.cpp
	# custom ImGui OpenVR backend
	${CMAKE_SOURCE_DIR}/src/extension/ImGui/backends/imgui_impl_openvr.cpp
	# ImPlot
	${ImPlot_ROOT}/implot.cpp
	${ImPlot_ROOT}/implot_items.cpp
)

target_include_directories(ImGui PUBLIC ${ImGui_ROOT} ${ImPlot_ROOT})
target_link_libraries(ImGui PUBLIC SDL3::SDL3 vulkan OpenVR::API)