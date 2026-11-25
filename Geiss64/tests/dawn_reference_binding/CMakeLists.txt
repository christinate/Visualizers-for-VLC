# Development-only Linux/macOS Node binding using the pinned shared SDK.
# No visualizer renderer, controller, or production package links this target.
cmake_minimum_required(VERSION 3.24)
project(OriginalJavaScriptDawnReference LANGUAGES CXX)
if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Darwin)$")
  message(FATAL_ERROR "Use Dawn's official Node target on Windows; this shared SDK recipe supports Linux/macOS")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
foreach(name DAWN_SOURCE DAWN_GENERATED NODE_GENERATED NODE_ADDON_API_DIR NODE_API_HEADERS_DIR DAWN_INSTALL)
  if(NOT IS_DIRECTORY "${${name}}")
    message(FATAL_ERROR "Set ${name} to the pinned dependency directory")
  endif()
endforeach()
file(READ "${DAWN_INSTALL}/build-provenance.json" provenance)
string(JSON revision GET "${provenance}" source_revision)
if(NOT revision STREQUAL "7daf3b0bee2c73621001ab15b256b5a84fb0b45f")
  message(FATAL_ERROR "The reference binding requires the renderer's pinned Dawn revision")
endif()
find_library(DAWN_REFERENCE_LIBRARY webgpu_dawn PATHS "${DAWN_INSTALL}/lib" NO_DEFAULT_PATH REQUIRED)
file(READ "${NODE_API_HEADERS_DIR}/symbols.js" symbols)
string(REGEX MATCHALL "napi_[a-z0-9_]*" symbols "${symbols}")
list(REMOVE_DUPLICATES symbols)
list(TRANSFORM symbols PREPEND "NAPI_SYMBOL(")
list(TRANSFORM symbols APPEND ")\n")
string(REPLACE ";" "" symbols "${symbols}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/NapiSymbols.h" "${symbols}")
file(GLOB binding_sources CONFIGURE_DEPENDS "${DAWN_SOURCE}/src/dawn/node/binding/*.cpp")
add_library(dawn_reference SHARED ${binding_sources}
  "${DAWN_SOURCE}/src/dawn/node/Module.cpp"
  "${DAWN_SOURCE}/src/dawn/node/ManualConstructors.cpp"
  "${DAWN_SOURCE}/src/dawn/node/NapiSymbols.cpp"
  "${DAWN_SOURCE}/src/dawn/node/interop/Core.cpp"
  "${NODE_GENERATED}/src/dawn/node/interop/WebGPU.cpp"
  "${DAWN_GENERATED}/src/dawn/dawn_proc.cpp"
  "${DAWN_GENERATED}/src/dawn/utils/ComboLimits.cpp"
  "${DAWN_SOURCE}/src/utils/assert.cc"
  "${DAWN_SOURCE}/src/utils/log.cc")
target_include_directories(dawn_reference PRIVATE
  "${DAWN_SOURCE}" "${DAWN_SOURCE}/include" "${DAWN_INSTALL}/include"
  "${DAWN_GENERATED}/src" "${DAWN_GENERATED}/include"
  "${NODE_GENERATED}" "${CMAKE_CURRENT_BINARY_DIR}"
  "${NODE_ADDON_API_DIR}" "${NODE_API_HEADERS_DIR}/include")
target_compile_definitions(dawn_reference PRIVATE NAPI_SYMBOL_WEAK_DECL_ONLY DAWN_HAVE_UTILS_BUILD_CONFIG)
target_link_libraries(dawn_reference PRIVATE "${DAWN_REFERENCE_LIBRARY}")
if(APPLE)
  # Same Node-provided N-API symbol policy as pinned upstream dawn_node.
  target_link_options(dawn_reference PRIVATE "-Wl,-undefined,dynamic_lookup")
endif()
set_target_properties(dawn_reference PROPERTIES PREFIX "" SUFFIX ".node"
  BUILD_RPATH "${DAWN_INSTALL}/lib")
