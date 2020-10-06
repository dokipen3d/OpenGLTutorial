# The name of our project
project(OpenGLTutorial)

# we want to use a recent version of cmake
cmake_minimum_required(VERSION 3.18)

# makes sure we have dependencies on our machine. sets variables for us
find_package(OpenGL REQUIRED)
find_package(glbinding CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(Assimp CONFIG REQUIRED)

# takes the files in the src directory and adds them to a variable called SRC_LIST
aux_source_directory(src/ SRC_LIST)

# tells cmake that we are making an exectutable program 
# whos source is from the files in the SRC_LIST variable
add_executable(chapter2 src/chapter2_windowCreation.cpp)
add_executable(chapter3 src/chapter3_helloTriangle.cpp)
add_executable(chapter4 src/chapter4_errorHandling.cpp)

# tells the compiler to use c++ 11 
set_property(TARGET chapter2 chapter3 chapter4 PROPERTY CXX_STANDARD 11)

set(LIBRARIES fmt::fmt
    fmt::fmt-header-only
    glfw
    assimp::assimp
    libpystring
    ${OPENGL_LIBRARIES}
    glbinding::glbinding
    glbinding::glbinding-aux)
# which libraries our program must link against

target_link_libraries(chapter2 PRIVATE ${LIBRARIES})
target_link_libraries(chapter3 PRIVATE ${LIBRARIES})
target_link_libraries(chapter4 PRIVATE ${LIBRARIES})
