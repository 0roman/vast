#!/bin/bash

git clone https://github.com/gabime/spdlog.git
cd spdlog
#not compiling in tags version v1.5.0 currently, just use master
#git checkout tags/v1.5.0
CC=/opt/ci-fuzz-2.31.0/bin/clang CXX=/opt/ci-fuzz-2.31.0/bin/clang++ CXXFLAGS="-stdlib=libc++ -fPIC" VERBOSE=1 cmake -B build -DCMAKE_BUILD_TYPE=release
cd build 
make -j8
make install
cd ../..
