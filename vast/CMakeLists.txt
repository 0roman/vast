option(VAST_ENABLE_VAST "Build the standalone vast binary"
       "${VAST_IS_NOT_SUBPROJECT}")
add_feature_info("VAST_ENABLE_VAST" VAST_ENABLE_VAST
                 "build the standalone vast binary.")

if (NOT VAST_ENABLE_VAST)
  return()
endif ()

add_executable(vast vast.cpp)
VASTTargetEnableTooling(vast)
target_link_libraries(vast PRIVATE vast::internal vast::libvast)
VASTTargetLinkWholeArchive(vast PRIVATE vast::libvast_native_plugins)
install(TARGETS vast DESTINATION "${CMAKE_INSTALL_BINDIR}")
add_executable(vast::vast ALIAS vast)

# Install vast in PREFIX/lib and headers in PREFIX/include/vast.
install(
  TARGETS vast
  EXPORT VASTTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# -- example configuration file -----------------------------------------------

VASTInstallExampleConfiguration(vast "${PROJECT_SOURCE_DIR}/vast.yaml.example"
                                "" "vast.yaml")

# -- init system integration ---------------------------------------------------

# TODO: This should be behind an option, and work for Linux as well.

if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
  # Install rc.d script on FreeBSD into PREFIX/etc/rc.d.
  # TODO: Technically this can break for relocatable binaries when the install
  # prefix at build-time is different than the install prefix at install-time.
  # The macOS installation below handles this correctly, and the FreeBSD
  # installation should be adapted to work similarly.
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/services/rc.d/vast.in"
                 "${CMAKE_CURRENT_BINARY_DIR}/services/rc.d/vast" @ONLY)
  install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/services/rc.d/vast"
    DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/rc.d"
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_EXECUTE)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  # Install launchd script on macOS into ~/Library/LaunchAgents
  install(
    CODE "\
    if (NOT IS_ABSOLUTE \"\${CMAKE_INSTALL_PREFIX}\")
      string(PREPEND CMAKE_INSTALL_PREFIX \"${CMAKE_SOURCE_DIR}/\")
    endif ()
    set(VAST_BINARY \"\${CMAKE_INSTALL_PREFIX}/bin/$<TARGET_FILE_NAME:vast>\")
    set(VAST_WORKING_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/share/vast\")
    message(STATUS
      \"Installing: \$ENV{HOME}/Library/LaunchAgents/com.tenzir.vast.plist\")
    configure_file(
      \"${CMAKE_CURRENT_SOURCE_DIR}/services/launchd/com.tenzir.vast.plist.in\"
      \"\$ENV{HOME}/Library/LaunchAgents/com.tenzir.vast.plist\" @ONLY)")
endif ()

# -- integration tests ---------------------------------------------------------

# TODO: Hide integration test suite behind an option that is enabled by default.

file(
  GENERATE
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/integration-$<CONFIG>.sh"
  CONTENT
    "#!/bin/sh
     if ! command -v jq >/dev/null 2>&1; then
       >&2 echo 'failed to find jq in $PATH'
       exit 1
     fi
     base_dir=\"${CMAKE_CURRENT_SOURCE_DIR}/integration\"
     env_dir=\"${CMAKE_CURRENT_BINARY_DIR}/integration_env\"
     app=\"$<IF:$<BOOL:${VAST_ENABLE_RELOCATABLE_INSTALLATIONS}>,$<TARGET_FILE:vast::vast>,${CMAKE_INSTALL_FULL_BINDIR}/$<TARGET_FILE_NAME:vast::vast>>\"
     set -e
     if [ ! -f \"$env_dir/bin/activate\" ]; then
       python3 -m venv \"$env_dir\"
     fi
     . \"$env_dir/bin/activate\"
     python -m pip install --upgrade pip
     python -m pip install -r \"$base_dir/requirements.txt\"
     python \"$base_dir/integration.py\" \
      --app \"$app\" \
      --directory vast-integration-test \
      --set \"$base_dir\"/vast_integration_suite.yaml \
      \"$@\"
     $<$<BOOL:${VAST_ENABLE_LSVAST}>:
     rm -rf \"${CMAKE_CURRENT_BINARY_DIR}/lsvast-integration-test\"
     mkdir -p \"${CMAKE_CURRENT_BINARY_DIR}/lsvast-integration-test\"
     $app --bare-mode -N -d \"${CMAKE_CURRENT_BINARY_DIR}/lsvast-integration-test/vast.db\" \
       import -r \"$base_dir\"/data/suricata/eve.json suricata
     python \"$base_dir/integration.py\" \
      --app \"$<TARGET_FILE:lsvast>\" \
      --directory \"${CMAKE_CURRENT_BINARY_DIR}/lsvast-integration-test\" \
      --set \"$base_dir\"/lsvast_integration_suite.yaml \
      \"$@\">
    ")

add_custom_target(
  integration
  COMMAND /bin/sh "${CMAKE_CURRENT_BINARY_DIR}/integration-$<CONFIG>.sh" "-v"
          "DEBUG"
  USES_TERMINAL)

add_custom_target(
  link-integration-directory ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/share/vast"
  COMMAND
    ${CMAKE_COMMAND} -E create_symlink "${CMAKE_CURRENT_SOURCE_DIR}/integration"
    "${CMAKE_BINARY_DIR}/share/vast/integration"
  COMMENT "Linking integration test directory")

install(
  DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/integration"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/vast"
  PATTERN "integration/vast.yaml.example" EXCLUDE)

install(FILES "${PROJECT_SOURCE_DIR}/vast.yaml.example"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/vast/integration")

# -- add plugins ---------------------------------------------------------------

set(VAST_PLUGINS
    ""
    CACHE STRING "Specify a list of plugins to build with VAST")
cmake_dependent_option(
  VAST_ENABLE_STATIC_PLUGINS "Force plugins to be linked statically" OFF
  "NOT VAST_ENABLE_STATIC_EXECUTABLE" ON)
add_feature_info("VAST_ENABLE_STATIC_PLUGINS" VAST_ENABLE_STATIC_PLUGINS
                 "Force plugins to be linked statically.")
if (VAST_PLUGINS)
  list(SORT VAST_PLUGINS)
  foreach (plugin_source_dir IN LISTS VAST_PLUGINS)
    if (NOT IS_ABSOLUTE "${plugin_source_dir}")
      string(PREPEND plugin_source_dir "${PROJECT_SOURCE_DIR}/")
    endif ()
    get_filename_component(plugin_binary_dir "${plugin_source_dir}" NAME)
    string(PREPEND plugin_binary_dir "${PROJECT_BINARY_DIR}/plugins/")
    add_subdirectory("${plugin_source_dir}" "${plugin_binary_dir}")
  endforeach ()
endif ()

get_property(VAST_BUNDLED_PLUGINS GLOBAL
             PROPERTY "VAST_BUNDLED_PLUGINS_PROPERTY")
list(TRANSFORM VAST_BUNDLED_PLUGINS PREPEND "\"")
list(TRANSFORM VAST_BUNDLED_PLUGINS APPEND "\"")
list(JOIN VAST_BUNDLED_PLUGINS "," joined_bundled_plugins)
target_compile_definitions(
  vast PRIVATE "VAST_BUNDLED_PLUGINS=${joined_bundled_plugins}")

# -- man page ------------------------------------------------------------------

# TODO: Hide behind a feature flag that is enabled by default and require
# Pandoc to be available.
# TODO: Keep a copy of the man page in the repository such that we know the last
# edit date. If pandoc+git is available, rebuild the man page.

find_package(Pandoc)
if (PANDOC_FOUND)
  add_custom_command(
    TARGET vast
    POST_BUILD
    COMMAND "$<TARGET_FILE:vast>" --bare-mode manual >
            "${CMAKE_CURRENT_BINARY_DIR}/vast.1.md"
    COMMAND
      "${PANDOC}" -s -f markdown -t man "${CMAKE_CURRENT_BINARY_DIR}/vast.1.md"
      -o "${CMAKE_CURRENT_BINARY_DIR}/vast.1"
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/vast.1.md"
               "${CMAKE_CURRENT_BINARY_DIR}/vast.1"
    COMMENT "Generating vast.1"
    VERBATIM)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/vast.1"
          DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
endif ()
