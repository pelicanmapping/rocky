function(rocky_install_export_files TARGET)

    include(CMakePackageConfigHelpers)

    #set(PACKAGE_INSTALL_DIR share/rocky)
    set(ROCKY_INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR})
    set(ROCKY_LIBRARY_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR})
    set(ROCKY_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/rocky")

    configure_package_config_file(
        "${TARGET}-config.cmake.in"
        "${TARGET}-config.cmake"
        INSTALL_DESTINATION ${ROCKY_CMAKE_INSTALL_DIR}
        PATH_VARS ROCKY_INCLUDE_INSTALL_DIR ROCKY_LIBRARY_INSTALL_DIR) 

    write_basic_package_version_file(
        "${TARGET}-config-version.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion)

    install(
        EXPORT ${TARGET}Targets
        FILE ${TARGET}-targets.cmake
        NAMESPACE rocky::
        DESTINATION ${ROCKY_CMAKE_INSTALL_DIR} )
        
    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-config.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}-config-version.cmake"
        DESTINATION
             ${ROCKY_CMAKE_INSTALL_DIR} )
    
endfunction()
