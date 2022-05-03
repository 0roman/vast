#!/bin/bash

apt-get update -y
apt-get install libpthread-stubs0-dev -y
git clone https://github.com/apache/arrow.git
cd arrow
git checkout apache-arrow-7.0.0.dev
cd cpp
CC=/opt/ci-fuzz-2.31.0/bin/clang CXX=/opt/ci-fuzz-2.31.0/bin/clang++ CXXFLAGS="-stdlib=libc++" VERBOSE=1 cmake -B build -DCMAKE_BUILD_TYPE=release -DARROW_COMPUTE=ON
cd build 
make -j8
make install
cd ../..
