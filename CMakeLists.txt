cmake_minimum_required(VERSION 3.16)
project(Booker)

include_directories(
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)

file(GLOB all_SRCS
        "${PROJECT_SOURCE_DIR}/include/*.hpp"
        "${PROJECT_SOURCE_DIR}/include/connector/input/*.hpp"
        "${PROJECT_SOURCE_DIR}/include/connector/trade/*.hpp"
        "${PROJECT_SOURCE_DIR}/include/strategy/*.hpp"
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/connector/input/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/connector/trade/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/strategy/*.cpp"
        )


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED True)
find_package(Poco REQUIRED Net NetSSL Util JSON Foundation )


add_executable(Booker ${all_SRCS})

target_link_libraries(Booker  PUBLIC Poco::Net Poco::NetSSL Poco::Util Poco::JSON Poco::Foundation)
