cmake_minimum_required(VERSION 3.5)

project(Blackhole)

set(OpenGL_GL_PREFERENCE "GLVND")
add_definitions(-DGLM_ENABLE_EXPERIMENTAL)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

find_package(OpenGL REQUIRED)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/glfw)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/glm)
find_package(GLEW REQUIRED)

file(GLOB SRC_FILES
  "${PROJECT_SOURCE_DIR}/include/*.h"
  "${PROJECT_SOURCE_DIR}/src/*.cpp"
  "${PROJECT_SOURCE_DIR}/src/*.c"
  "${PROJECT_SOURCE_DIR}/thirdparty/imgui/*.cpp"
  "${PROJECT_SOURCE_DIR}/src/*.cc")
  
add_executable(${CMAKE_PROJECT_NAME} ${SRC_FILES})
  
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE glfw GLEW::GLEW glm::glm OpenGL::GL)

target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_17)

# --- NVML Support ---
option(USE_NVML "Enable NVIDIA Management Library support" ON)
if(USE_NVML)
    # Find NVML headers
    find_path(NVML_INCLUDE_DIR nvml.h PATHS /usr/include /usr/local/include /usr/lib/wsl/include)
    if(NVML_INCLUDE_DIR)
        message(STATUS "Found NVML headers: ${NVML_INCLUDE_DIR}")
        target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${NVML_INCLUDE_DIR})
    else()
        message(WARNING "NVML headers not found! Please install libnvidia-ml-dev.")
    endif()

    # Find the NVML library
    find_library(NVML_LIBRARY nvidia-ml PATHS /usr/lib /usr/lib/wsl /usr/local/lib)
    if(NVML_LIBRARY)
        message(STATUS "Found NVML: ${NVML_LIBRARY}")
        add_definitions(-DUSE_NVML)
        target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${NVML_LIBRARY})
    else()
        message(WARNING "NVML not found! Falling back to nvidia-smi method.")
    endif()
endif()

add_custom_command(
  TARGET ${CMAKE_PROJECT_NAME}
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/assets"
          "$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/assets"
)

add_custom_command(
  TARGET ${CMAKE_PROJECT_NAME}
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/shader/"
          "$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/shader"
)