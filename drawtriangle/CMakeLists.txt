
add_library(vk_lib SHARED IMPORTED)
set_target_properties(vk_lib PROPERTIES IMPORTED_LOCATION
        ${CMAKE_CURRENT_SOURCE_DIR}/../moltenvk/macOS/dynamic/libMoltenVK.dylib)

link_libraries(glfw)
link_libraries(vk_lib)


include_directories(${glfw_INCLUDE_DIRS}
        "${GLFW_SOURCE_DIR}/deps"
        ${CMAKE_CURRENT_SOURCE_DIR}/../moltenvk/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../glm
        ${CMAKE_CURRENT_SOURCE_DIR}/../
        )

if (MATH_LIBRARY)
    link_libraries("${MATH_LIBRARY}")
endif()

if (MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

if (GLFW_USE_OSMESA)
    add_definitions(-DUSE_NATIVE_OSMESA)
endif()


set(GLAD_GL "${GLFW_SOURCE_DIR}/deps/glad/gl.h"
            "${GLFW_SOURCE_DIR}/deps/glad_gl.c")
set(GETOPT "${GLFW_SOURCE_DIR}/deps/getopt.h"
           "${GLFW_SOURCE_DIR}/deps/getopt.c")
set(TINYCTHREAD "${GLFW_SOURCE_DIR}/deps/tinycthread.h"
                "${GLFW_SOURCE_DIR}/deps/tinycthread.c")

add_executable(drawtriangl
        main.cpp
        ${GLAD_GL}
        )


