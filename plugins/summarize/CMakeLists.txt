cmake_minimum_required(VERSION 3.18...3.23 FATAL_ERROR)

project(
  summarize
  VERSION 2.0.0
  DESCRIPTION "Summarize transform plugin for VAST"
  LANGUAGES CXX)

# Enable unit testing. Note that it is necessary to include CTest in the
# top-level CMakeLists.txt file for it to create a test target, so while
# optional for plugins built alongside VAST, it is necessary to specify this
# line manually so plugins can be linked against an installed VAST.
include(CTest)

find_package(VAST REQUIRED)
VASTRegisterPlugin(
  TARGET summarize
  ENTRYPOINT summarize.cpp
  TEST_SOURCES tests/summarize.cpp)
