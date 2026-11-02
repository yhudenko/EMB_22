cmake_minimum_required(VERSION 3.16.0)

list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/lib/eaf"
    "${CMAKE_CURRENT_LIST_DIR}/lib/traffic_lights"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(TrafficLightsEspIdf)
