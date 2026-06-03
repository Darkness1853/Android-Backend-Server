cmake_minimum_required(VERSION 3.10)
project(LocationServer)

set(OpenGL_GL_PREFERENCE "GLVND")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/third_party)

set(IMGUI_DIR ${THIRD_PARTY_DIR}/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

set(IMPLOT_DIR ${THIRD_PARTY_DIR}/implot)
set(IMPLOT_SOURCES
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
)

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)
pkg_check_modules(GLEW REQUIRED glew)
pkg_check_modules(GLFW REQUIRED glfw3)
pkg_check_modules(NLOHMANN_JSON REQUIRED nlohmann_json)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(STB REQUIRED stb)
pkg_check_modules(PQXX REQUIRED libpqxx)

find_package(PostgreSQL REQUIRED)

set(PROJECT_SOURCES
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/server.cpp
    ${SRC_DIR}/gui.cpp
    ${SRC_DIR}/db_client.cpp
    ${SRC_DIR}/tile_manager.cpp
    ${SRC_DIR}/heatmap.cpp
    ${SRC_DIR}/map.cpp
    ${IMGUI_SOURCES}
    ${IMPLOT_SOURCES}
)

set(PROJECT_HEADERS
    ${INCLUDE_DIR}/location.hpp
    ${INCLUDE_DIR}/server.hpp
    ${INCLUDE_DIR}/gui.hpp
    ${INCLUDE_DIR}/db_client.hpp
    ${INCLUDE_DIR}/heatmap.hpp
    ${INCLUDE_DIR}/tile_manager.hpp
    ${INCLUDE_DIR}/map.hpp
    ${INCLUDE_DIR}/MapPoint.hpp
)

add_executable(location_server ${PROJECT_SOURCES} ${PROJECT_HEADERS}
    src/heatmap.cpp)

target_include_directories(location_server PRIVATE
    ${INCLUDE_DIR}
    ${ZMQ_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
    ${NLOHMANN_JSON_INCLUDE_DIRS}
    ${PostgreSQL_INCLUDE_DIRS}
    ${PQXX_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${STB_INCLUDE_DIRS}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMPLOT_DIR}
)

target_link_libraries(location_server
    ${ZMQ_LIBRARIES}
    ${GLEW_LIBRARIES}
    ${GLFW_LIBRARIES}
    ${NLOHMANN_JSON_LIBRARIES}
    ${PostgreSQL_LIBRARIES}
    ${PQXX_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${CURL_LIBRARIES}
    ${STB_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    pthread
    dl
    X11
)

target_compile_definitions(location_server PRIVATE
    IMGUI_IMPL_OPENGL_LOADER_GLEW
)