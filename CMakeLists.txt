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
find_package(tinyobjloader CONFIG REQUIRED)
find_package(OpenMP)

# takes the files in the src directory and adds them to a variable called SRC_LIST
aux_source_directory(src/ SRC_LIST)

# tells cmake that we are making an exectutable program 
# whos source is from the files in the SRC_LIST variable
add_executable(chapter1_HelloWorld src/chapter1_HelloWorld.cpp)
add_executable(chapter2_windowCreation src/chapter2_windowCreation.cpp)
add_executable(chapter3_helloTriangle src/chapter3_helloTriangle.cpp)
add_executable(chapter4_errorHandling src/chapter4_errorHandling.cpp)
add_executable(chapter5_vertexData src/chapter5_vertexData.cpp)
add_executable(chapter6_multipleBuffers src/chapter6_multipleBuffers.cpp)
add_executable(chapter7_interleavingVertexData src/chapter7_interleavingVertexData.cpp)
add_executable(chapter8_multipleVAOs src/chapter8_multipleVAOs.cpp)
add_executable(chapter9_fullScreenEffectsShaderToy src/chapter9_fullScreenEffectsShaderToy.cpp)
add_executable(chapter10_sendingUniformDataToShaders src/chapter10_sendingUniformDataToShaders.cpp)
add_executable(chapter11_loading3DDataFromDisk src/chapter11_loading3DDataFromDisk.cpp)
add_executable(chapter12_shaderTransforms src/chapter12_shaderTransforms.cpp)
add_executable(chapter13_elementBuffers src/chapter13_elementBuffers.cpp)
add_executable(chapter14_textures src/chapter14_textures.cpp)
add_executable(chapter15_basicDiffuseLighting src/chapter15_basicDiffuseLighting.cpp)
add_executable(chapter16_multipleTextures src/chapter16_multipleTextures.cpp)
add_executable(chapter17_textureArrays src/chapter17_textureArrays.cpp)
add_executable(chapter18_drawIndirect src/chapter18_drawIndirect.cpp)
add_executable(chapter19_multiDrawIndexingBuffers src/chapter19_multiDrawIndexingBuffers.cpp)

add_executable(testObj src/test_obj_loader.cpp)


# tells the compiler to use c++ 11 
set_property(GLOBAL PROPERTY CXX_STANDARD 11)

set(LIBRARIES fmt::fmt
    fmt::fmt-header-only
    glfw
    ${OPENGL_LIBRARIES}
    glbinding::glbinding
    glbinding::glbinding-aux
    ${STB_INCLUDE_DIRS}
    )
# which libraries our program must link against
target_link_libraries(chapter1_HelloWorld PRIVATE ${LIBRARIES})
target_link_libraries(chapter2_windowCreation PRIVATE ${LIBRARIES})
target_link_libraries(chapter3_helloTriangle PRIVATE ${LIBRARIES})
target_link_libraries(chapter4_errorHandling PRIVATE ${LIBRARIES})
target_link_libraries(chapter5_vertexData PRIVATE ${LIBRARIES})
target_link_libraries(chapter6_multipleBuffers PRIVATE ${LIBRARIES})
target_link_libraries(chapter7_interleavingVertexData PRIVATE ${LIBRARIES})
target_link_libraries(chapter8_multipleVAOs PRIVATE ${LIBRARIES})
target_link_libraries(chapter9_fullScreenEffectsShaderToy PRIVATE ${LIBRARIES})
target_link_libraries(chapter9_fullScreenEffectsShaderToy PRIVATE ${LIBRARIES})
target_link_libraries(chapter10_sendingUniformDataToShaders PRIVATE ${LIBRARIES})
target_link_libraries(chapter11_loading3DDataFromDisk PRIVATE ${LIBRARIES} PUBLIC OpenMP::OpenMP_CXX)
target_link_libraries(chapter12_shaderTransforms PRIVATE ${LIBRARIES} PUBLIC OpenMP::OpenMP_CXX)
target_link_libraries(chapter13_elementBuffers PRIVATE ${LIBRARIES} PUBLIC OpenMP::OpenMP_CXX)
target_link_libraries(chapter14_textures PRIVATE ${LIBRARIES} PUBLIC OpenMP::OpenMP_CXX)
target_link_libraries(chapter15_basicDiffuseLighting PRIVATE ${LIBRARIES} PUBLIC OpenMP::OpenMP_CXX)
target_link_libraries(chapter16_multipleTextures PRIVATE ${LIBRARIES} PUBLIC OpenMP::OpenMP_CXX)
target_link_libraries(chapter17_textureArrays PRIVATE ${LIBRARIES} PUBLIC OpenMP::OpenMP_CXX)
target_link_libraries(chapter18_drawIndirect PRIVATE ${LIBRARIES} PUBLIC OpenMP::OpenMP_CXX)
target_link_libraries(chapter19_multiDrawIndexingBuffers PRIVATE ${LIBRARIES} PUBLIC OpenMP::OpenMP_CXX)

target_link_libraries(testObj PRIVATE ${LIBRARIES} PUBLIC OpenMP::OpenMP_CXX)


