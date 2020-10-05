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

# takes the files in the src directory and adds them to a variable called SRC_LIST
aux_source_directory(src/ SRC_LIST)

# tells cmake that we are making an exectutable program 
# whos source is from the files in the SRC_LIST variable
add_executable(${PROJECT_NAME} ${SRC_LIST})

# tells the compiler to use c++ 11 
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 11)

# which libraries our program must link against
target_link_libraries(  ${PROJECT_NAME} 
                        PRIVATE 
                        fmt::fmt
                        fmt::fmt-header-only
                        glfw
                        ${OPENGL_LIBRARIES}
                        glbinding::glbinding
                        glbinding::glbinding-aux    # for additional, auxiliary 
                                                    # features as logging, meta information,
                                                    # or debugging functionality 
                        )

