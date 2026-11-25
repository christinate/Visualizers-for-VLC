# Independent Prism Flow API 4 module, using this repository's pinned SDK.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(PRISMFLOW_BUILD_CORE_PROBE OFF)
add_subdirectory(../prismflow "${CMAKE_CURRENT_BINARY_DIR}/core")
add_library(prismflow_image_import STATIC ../prismflow/image_import.cpp)
target_include_directories(prismflow_image_import PUBLIC ../prismflow)
target_link_libraries(prismflow_image_import PUBLIC prismflow_core visualizer_image Threads::Threads)
add_library(prismflow_dawn STATIC
  ../prismflow/gpu_support.cpp ../prismflow/gpu_dynamic.cpp ../prismflow/gpu_feedback.cpp
  ../prismflow/gpu_present.cpp ../prismflow/gpu_dots.cpp ../prismflow/gpu_bokeh.cpp
  ../prismflow/gpu_text.cpp ../prismflow/scene_dawn.cpp)
target_include_directories(prismflow_dawn PUBLIC ../prismflow)
target_link_libraries(prismflow_dawn PUBLIC visualizer_dawn prismflow_core visualizer_text prismflow_image_import visualizer_music)
if(MSVC)
  target_compile_options(prismflow_dawn PRIVATE /fp:strict)
else()
  target_compile_options(prismflow_dawn PRIVATE -ffp-contract=off)
endif()
add_library(prismflow_native MODULE ../prismflow/plugin_dawn.cpp)
target_include_directories(prismflow_native PRIVATE ../../sdk/native)
target_link_libraries(prismflow_native PRIVATE prismflow_dawn)
set_target_properties(prismflow_native PROPERTIES PREFIX "" OUTPUT_NAME "prismflow_dawn" ARCHIVE_OUTPUT_NAME "prismflow_dawn_import"
  BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "$ORIGIN/../../native")
