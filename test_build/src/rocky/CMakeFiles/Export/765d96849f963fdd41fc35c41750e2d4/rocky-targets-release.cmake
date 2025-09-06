#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "rocky::rocky" for configuration "Release"
set_property(TARGET rocky::rocky APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(rocky::rocky PROPERTIES
  IMPORTED_LINK_DEPENDENT_LIBRARIES_RELEASE "PROJ::proj"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/librocky.so.27"
  IMPORTED_SONAME_RELEASE "librocky.so.27"
  )

list(APPEND _cmake_import_check_targets rocky::rocky )
list(APPEND _cmake_import_check_files_for_rocky::rocky "${_IMPORT_PREFIX}/lib/librocky.so.27" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
