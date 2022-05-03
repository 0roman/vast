#!/bin/bash

git clone https://github.com/jbeder/yaml-cpp
cd yaml-cpp
CC=/opt/ci-fuzz-2.31.0/bin/clang CXX=/opt/ci-fuzz-2.31.0/bin/clang++ CXXFLAGS="-stdlib=libc++" VERBOSE=1 cmake -B build -DCMAKE_BUILD_TYPE=release
cd build 
make -j8
make install
#DESTDIR="/usr/lib/x86_64-linux-gnu/" make install
cd ../..