cmake_minimum_required(VERSION 3.8)
project(WhisperNetTransferSystem LANGUAGES C CXX)

find_package(Boost)
add_subdirectory("thirdparty/libzt")

set(CMAKE_BUILD_TYPE Debug)
set(CXX_STANDARD 17)

set(thirdparty "thirdparty/")
set(ztcpppath "${thirdparty}/ztcpp/ZTCpp/")

# Build c++ wrapper around zeroteir
add_compile_definitions("ZTCPP_EXPORT")
add_library(ztcpp "${ztcpppath}Source/Events.cpp" "${ztcpppath}Source/Ip_address.cpp" "${ztcpppath}Source/Service.cpp" "${ztcpppath}Source/Sockaddr_util.cpp" "${ztcpppath}Source/Socket.cpp")
target_include_directories(ztcpp PUBLIC "${ztcpppath}Include/" "${thirdparty}libzt/include")
target_link_libraries(ztcpp PUBLIC zt-static)

file(GLOB sources "src/*.cpp" "src/*.c")
set(includes "src/" "src/client" "src/server" ${Boost_INCLUDE_DIRS} "${thirdparty}" "${ztcpppath}Include/" "${thirdparty}libzt/include")
set(libraries ztcpp)

message("${STATIC_LIB_NAME}")

add_executable (wnts ${sources} ${clientSources})
target_include_directories (wnts PUBLIC ${includes})
target_link_libraries (wnts LINK_PUBLIC ${libraries})