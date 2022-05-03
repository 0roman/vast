if(DEFINED CIFUZZ_INSTALL_ROOT)
  
  # Add CI FUzz installation path to make sure that we can load the CIFuzz package
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CIFUZZ_INSTALL_ROOT}/share/cmake")
  # Load the package
  find_package(CIFuzz)

  # In this examples all the fuzz tests will lie in the folder fuzz_targets
  file(GLOB targets "fuzz_targets/*.cpp")
  foreach(fuzzer_source ${targets})

    # Get the fuzz test file name without the extension
    get_filename_component(fuzzer ${fuzzer_source} NAME_WE)

    # add_cifuzz_test works like add_executable but fuzz test configuration
    add_cifuzz_test(${fuzzer}
      SOURCES     ${fuzzer_source}
      MAX_RUNTIME "30m"
      PARALLEL_EXECUTIONS "1"
      SEED_CORPUS "${CMAKE_CURRENT_SOURCE_DIR}/seeds/${fuzzer}"

      # Possible Parameters:
      #Single Value Parameters:
      # MAX_RUNTIME PARALLEL_EXECUTIONS SEED_CORPUS DICTIONARY
      # Multi Value Parameters:
      # SOURCES LINK_LIBRARIES TARGET_ARGS ENGINE_ARGS ENGINE_ENV
    )
    
    # Other CMake methods can be used normally
    target_link_libraries(${fuzzer} PRIVATE vast::libvast vast::internal)

    # If you want to link a dynamic library use cifuzz_test_link_libraries
    # instead of target_link_libraries to make sure that the built library
    # gets bundled with the fuzz target. E.g.:
    #cifuzz_test_link_libraries(${fuzzer} vast::libvast vast::internal)

  endforeach()

endif()
