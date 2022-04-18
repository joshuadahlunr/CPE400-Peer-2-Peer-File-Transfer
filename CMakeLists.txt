cmake_minimum_required(VERSION 3.8)
project(WhisperNetTransferSystem LANGUAGES C CXX)

find_package(Boost REQUIRED system)
find_package(Threads)
add_subdirectory("thirdparty/libzt")

set(CMAKE_BUILD_TYPE Debug)
set(CXX_STANDARD 17 REQUIRED)
add_compile_options(-std=c++17) # Needed to compile with older versions of GCC

set(thirdparty "thirdparty/")
set(ztcpppath "${thirdparty}/ztcpp/ZTCpp/")

# Build c++ wrapper around zeroteir
add_compile_definitions("ZTCPP_EXPORT")
add_library(ztcpp "${ztcpppath}Source/Events.cpp" "${ztcpppath}Source/Ip_address.cpp" "${ztcpppath}Source/Service.cpp" "${ztcpppath}Source/Sockaddr_util.cpp" "${ztcpppath}Source/Socket.cpp")
target_include_directories(ztcpp PUBLIC "${ztcpppath}Include/" "${thirdparty}libzt/include" "${thirdparty}dtl" ${Boost_INCLUDE_DIRS})
target_link_libraries(ztcpp PUBLIC zt-static Threads::Threads)

# Command line parser
add_subdirectory("${thirdparty}Argos")

file(GLOB sources "src/*.cpp" "src/*.c")
set(includes "src/" "src/client" "src/server" ${Boost_INCLUDE_DIRS} "${thirdparty}" "${ztcpppath}Include/" "${thirdparty}libzt/include" "${thirdparty}cereal/include")
set(libraries Threads::Threads ztcpp Argos ${Boost_LIBRARIES})

add_executable (wnts ${sources} ${clientSources})
target_include_directories (wnts PUBLIC ${includes})
target_link_libraries (wnts LINK_PUBLIC ${libraries})