# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.24)
project(CoralCanyon LANGUAGES CXX)
option(VIS_BUILD_DAWN_RUNTIME "Build the native visualizer and runtime" ON)
add_subdirectory("../Terrain64" terrain)
