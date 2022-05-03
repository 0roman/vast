CXXFLAGS="-stdlib=libc++" VERBOSE=1 cmake -B $WORK_DIR -DCIFUZZ_INSTALL_ROOT=/opt/ci-fuzz-2.31.0
VERBOSE=1 cmake --build $WORK_DIR --parallel -j8 --target query_parser