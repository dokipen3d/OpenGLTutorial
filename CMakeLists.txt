# The name of our project
project(OpenGLTutorial)

# we want to use a recent version of cmake
cmake_minimum_required(VERSION 3.18)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE})

# for chapter 8 we need a file to load
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/models/rubberToy.obj
    DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/textures/toylowres.jpg
    DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/textures/body_diffuse.jpg
    DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/textures/tankTops_pants_boots_diffuse.jpg
    DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# makes sure we have dependencies on our machine. sets variables for us
find_package(OpenGL REQUIRED)
find_package(glbinding CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(Assimp CONFIG REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)
find_package(OpenMP)
find_package(absl CONFIG REQUIRED)

# takes the files in the src directory and adds them to a variable called SRC_LIST
aux_source_directory(src/ SRC_LIST)

# tells cmake that we are making an exectutable program 
# whos source is from the files in the SRC_LIST variable
add_executable(chapter1 src/chapter1_HelloWorld.cpp)
add_executable(chapter2 src/chapter2_windowCreation.cpp)
add_executable(chapter3 src/chapter3_helloTriangle.cpp)
add_executable(chapter4 src/chapter4_errorHandling.cpp)
add_executable(chapter5 src/chapter5_vertexData.cpp)
add_executable(chapter6 src/chapter6_multipleBuffers.cpp)
add_executable(chapter7 src/chapter7_interleavingVertexData.cpp)
add_executable(chapter8 src/chapter8_loading3DDataFromDisk.cpp)
add_executable(chapter9 src/chapter9_shaderTransforms.cpp)
add_executable(chapter10 src/chapter10_elementBuffers.cpp)
add_executable(chapter11 src/chapter11_textures.cpp)
add_executable(chapter13 src/chapter13_multipleTextures.cpp)
add_executable(chapter14 src/chapter14_textureArrays.cpp)
add_executable(testObj src/test_obj_loader.cpp)


# tells the compiler to use c++ 11 
set_property(GLOBAL PROPERTY CXX_STANDARD 11)

set(LIBRARIES fmt::fmt
    fmt::fmt-header-only
    glfw
    libpystring
    ${OPENGL_LIBRARIES}
    glbinding::glbinding
    glbinding::glbinding-aux
    ${STB_INCLUDE_DIRS}
    absl::any absl::base absl::bits absl::city
    )
# which libraries our program must link against
target_link_libraries(chapter1 PRIVATE ${LIBRARIES})
target_link_libraries(chapter2 PRIVATE ${LIBRARIES})
target_link_libraries(chapter3 PRIVATE ${LIBRARIES})
target_link_libraries(chapter4 PRIVATE ${LIBRARIES})
target_link_libraries(chapter5 PRIVATE ${LIBRARIES})
target_link_libraries(chapter6 PRIVATE ${LIBRARIES})
target_link_libraries(chapter7 PRIVATE ${LIBRARIES})
target_link_libraries(chapter8 PRIVATE ${LIBRARIES} PUBLIC OpenMP::OpenMP_CXX)
target_link_libraries(chapter9 PRIVATE ${LIBRARIES} PUBLIC OpenMP::OpenMP_CXX)
target_link_libraries(chapter10 PRIVATE ${LIBRARIES} PUBLIC OpenMP::OpenMP_CXX)
target_link_libraries(chapter11 PRIVATE ${LIBRARIES} PUBLIC OpenMP::OpenMP_CXX)
target_link_libraries(chapter13 PRIVATE ${LIBRARIES} PUBLIC OpenMP::OpenMP_CXX)
target_link_libraries(chapter14 PRIVATE ${LIBRARIES} PUBLIC OpenMP::OpenMP_CXX)


target_link_libraries(testObj PRIVATE ${LIBRARIES} PUBLIC OpenMP::OpenMP_CXX)


