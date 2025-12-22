cmake_minimum_required(VERSION 3.26)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

project(openvr_openfps)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED ENABLE_VULKAN_VALIDATION)
    set(ENABLE_VULKAN_VALIDATION OFF CACHE BOOL "Enable Vulkan validation layers" FORCE)
endif()

# Renderer configuration

# Vulkan validation layer adds extra reporting that may also catch validation layers orginating from external sources, ie. SteamVR
set(ENABLE_VULKAN_VALIDATION OFF)

set(OpenVR_ROOT ${CMAKE_SOURCE_DIR}/3rdparty/OpenVR)
set(GLM_ROOT ${CMAKE_SOURCE_DIR}/3rdparty/glm)
set(Json_ROOT ${CMAKE_SOURCE_DIR}/3rdparty/json)

find_package(OpenVR REQUIRED)

# Add GLM (avoiding an annoying target it adds by creating a dummy one with the same name).
add_custom_target(uninstall)
add_subdirectory(${GLM_ROOT})

# Include rest of the CMake bindings
add_subdirectory(3rdparty)

add_subdirectory(${Json_ROOT} EXCLUDE_FROM_ALL)

message(STATUS "ENABLE_VULKAN_VALIDATION = ${ENABLE_VULKAN_VALIDATION}")

add_executable(openfps
    "src/Main.cpp"
    "src/core/Settings.cpp"
    "src/core/TaskMonitor.cpp"
    "src/overlay/Overlay.cpp"
    "src/overlay/controller/ControllerOverlay.cpp"
    "src/overlay/dashboard/DashboardOverlay.cpp"
    "src/renderer/VulkanRenderer.cpp"
)

target_include_directories(openfps PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Windows executable icon
if (WIN32)
    target_sources(openfps PRIVATE "resources/winicon.rc")
endif()

target_compile_features(openfps PUBLIC cxx_std_23)

target_link_libraries(openfps
    PRIVATE
        OpenVR::API
        ImGui
        glm::glm
        nlohmann_json::nlohmann_json
)

if (ENABLE_VULKAN_VALIDATION)
    add_definitions(-DENABLE_VULKAN_VALIDATION)
endif()

set(CUSTOM_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/bin")

add_custom_target(openfps_resources)

add_custom_command(
    TARGET openfps_resources POST_BUILD
    COMMENT "Copying Resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/resources ${CUSTOM_OUTPUT_DIR}
)

add_dependencies(openfps openfps_resources)

add_custom_command(
    TARGET openfps POST_BUILD
    COMMENT "Copying Executable and DLLs"
    COMMAND ${CMAKE_COMMAND} -E copy -t ${CUSTOM_OUTPUT_DIR} $<TARGET_FILE:openfps>
    COMMAND ${CMAKE_COMMAND} -E copy -t ${CUSTOM_OUTPUT_DIR} $<TARGET_RUNTIME_DLLS:openfps>
    COMMAND_EXPAND_LISTS
)