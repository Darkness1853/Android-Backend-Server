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

set(PROJECT_SOURCES
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/server.cpp
    ${SRC_DIR}/gui.cpp
    ${IMGUI_SOURCES}
    ${IMPLOT_SOURCES}
)

set(PROJECT_HEADERS
    ${INCLUDE_DIR}/location.hpp
    ${INCLUDE_DIR}/server.hpp
    ${INCLUDE_DIR}/gui.hpp
)

add_executable(location_server ${PROJECT_SOURCES} ${PROJECT_HEADERS})

target_include_directories(location_server PRIVATE
    ${INCLUDE_DIR}
    ${ZMQ_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
    ${NLOHMANN_JSON_INCLUDE_DIRS}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMPLOT_DIR}
)

target_link_libraries(location_server
    ${ZMQ_LIBRARIES}
    ${GLEW_LIBRARIES}
    ${GLFW_LIBRARIES}
    ${NLOHMANN_JSON_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    pthread
    dl
    X11
)

target_compile_definitions(location_server PRIVATE
    IMGUI_IMPL_OPENGL_LOADER_GLEW
)