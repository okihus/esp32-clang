cmake_minimum_required(VERSION 3.16)
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;sdkconfig.defaults.local")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(hello)
