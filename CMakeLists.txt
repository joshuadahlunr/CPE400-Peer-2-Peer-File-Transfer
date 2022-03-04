cmake_minimum_required(VERSION 3.8)
project(ztpinger LANGUAGES C CXX)

find_package(Boost)
add_subdirectory("thirdparty/libzt")

set(CMAKE_BUILD_TYPE Debug)
set(CXX_STANDARD 20)

set(thirdparty "thirdparty/")
set(ztcpppath "${thirdparty}/ztcpp/ZTCpp/")

# Build c++ wrapper around zeroteir
set(ZTCPP_LIBRARY_NAME "ztcpp")
add_compile_definitions("ZTCPP_EXPORT")
add_library(${ZTCPP_LIBRARY_NAME} "${ztcpppath}Source/Events.cpp" "${ztcpppath}Source/Ip_address.cpp" "${ztcpppath}Source/Service.cpp" "${ztcpppath}Source/Sockaddr_util.cpp" "${ztcpppath}Source/Socket.cpp")
target_include_directories(${ZTCPP_LIBRARY_NAME} PUBLIC "${ztcpppath}Include/" "${thirdparty}libzt/include")
target_link_libraries(${ZTCPP_LIBRARY_NAME} PRIVATE zt-static)

file(GLOB sources "src/*.cpp" "src/*.c")
file(GLOB clientSources "src/client/*.cpp" "src/client/*.c")
file(GLOB serverSources "src/server/*.cpp" "src/server/*.c")
set(includes "src/" "src/client" "src/server" ${Boost_INCLUDE_DIRS} "${ztcpppath}Include/" "${thirdparty}libzt/include")
set(libraries ${ZTCPP_LIBRARY_NAME})

message("${STATIC_LIB_NAME}")

add_executable (ztclient ${sources} ${clientSources})
target_include_directories (ztclient PUBLIC ${includes})
target_link_libraries (ztclient LINK_PUBLIC ${libraries})

add_executable (ztserver ${sources} ${serverSources})
target_include_directories (ztserver PUBLIC ${includes})
target_link_libraries (ztserver LINK_PUBLIC ${libraries})