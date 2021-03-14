# OpenGLTutorial

##ubuntu
### mesa drivers ( for llvmpipe)
sudo add-apt-repository ppa:oibaf/graphics-drivers
sudo apt-get update
sudo apt-get upgrade

### mesa dev
sudo apt install mesa-utils
sudo apt install libgl1-mesa-dev
sudo apt install libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev

### glbindings (vcpkg seems broken so use apt)

sudo apt-get install libglbinding-dev
### 
./vcpkg install glfw3
/vcpkg install glm
/vcpkg install fmt
/vcpkg install glm
/vcpkg install pysting
/vcpkg install stb


/vcpkg install tinyobjloader
./vcpkg integrate install
cmake .. -DCMAKE_TOOLCHAIN_FILE=/home/dokipen/Documents/projects/vcpkg/scripts/buildsystems/vcpkg.cmake
