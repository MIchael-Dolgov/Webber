# Roadmap:
* multithreaded packages sending
* unit tests by gtest
* more plugable sockets.cpp classes
* http body attachments packages sending
# How to build project:
### Step 1 (clone repo)
> git clone https://github.com/MIchael-Dolgov/Webber/tree/main
### Step2 (create cmake build folder)
> cd Webber && mkdir build && cd build
### Step 3 (add cmake compiling config)
> cmake ..  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release
### Step 4 (compile project)
> cmake --build && make
### Step 5 (move bin to project root directory)
> mv Webber .. && cd ..
### Step 6 (make file executable)
> chmod -x Webber
### Run
> ./Webber