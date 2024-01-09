cmake_minimum_required(VERSION 3.0.0)
project(analysis_support)


file(GLOB_RECURSE analysis_support_CPPS *.cpp)
add_library(analysis_support SHARED ${analysis_support_CPPS})
target_link_libraries(analysis_support PUBLIC satdump_core)
target_include_directories(analysis_support PUBLIC src)

install(TARGETS analysis_support DESTINATION lib/satdump/plugins)
