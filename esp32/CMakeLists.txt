# ESP-IDF project overlay for NUT.
# Run from this directory: idf.py set-target esp32s3 && idf.py build
# NUT sources stay in the repository root so they can be rebased onto upstream.

cmake_minimum_required(VERSION 3.22)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(nut-esp32)
