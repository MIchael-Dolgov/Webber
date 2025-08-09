# Roadmap:
* multithreaded packages sending
* unit tests by gtest
* more plugable sockets.cpp classes
* http body attachments packages sending
* encrypted transfer with ssl && HTTPS support
* file compressing by gzip
# How to build project:
### Step 1 (clone repo)
> git clone https://github.com/MIchael-Dolgov/Webber/tree/main
### Step2 (create cmake build folder)
* (make sure what cmake and make are installed in your Linux system)
> cd Webber && mkdir build && cd build
### Step 3 (add cmake compiling config)
> cmake ..  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release
### Step 4 (compile project)
> cmake --build && make
### Step 5 (move bin to project root directory)
> mv webber .. && cd ..
### Step 6 (make file executable)
> chmod -x webber
### Run
> ./webber
