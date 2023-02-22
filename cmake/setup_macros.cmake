#
# macros provided by the core library (adapted from vsgMacros)
#

# give hint for cmake developers
if(NOT _rocky_macros_included)
    set(_rocky_macros_included 1)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})
endif()

#
# setup build related variables
#
macro(setup_build_vars)
    set(CMAKE_DEBUG_POSTFIX "d" CACHE STRING "add a postfix, usually d on windows")
    set(CMAKE_RELEASE_POSTFIX "" CACHE STRING "add a postfix, usually empty on windows")
    set(CMAKE_RELWITHDEBINFO_POSTFIX "" CACHE STRING "add a postfix, usually empty on windows")
    set(CMAKE_MINSIZEREL_POSTFIX "s" CACHE STRING "add a postfix, usually empty on windows")

    # Change the default build type to Release
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
    endif(NOT CMAKE_BUILD_TYPE)


    if(CMAKE_COMPILER_IS_GNUCXX)
        set(VSG_WARNING_FLAGS -Wall -Wparentheses -Wno-long-long -Wno-import -Wreturn-type -Wmissing-braces -Wunknown-pragmas -Wmaybe-uninitialized -Wshadow -Wunused -Wno-misleading-indentation -Wextra CACHE STRING "Compile flags to use.")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(VSG_WARNING_FLAGS -Wall -Wparentheses -Wno-long-long -Wno-import -Wreturn-type -Wmissing-braces -Wunknown-pragmas -Wshadow -Wunused -Wextra CACHE STRING "Compile flags to use.")
    else()
        set(VSG_WARNING_FLAGS CACHE STRING "Compile flags to use.")
    endif()

    add_compile_options(${VSG_WARNING_FLAGS})

    # set upper case <PROJECT>_VERSION_... variables
    string(TOUPPER ${PROJECT_NAME} UPPER_PROJECT_NAME)
    set(${UPPER_PROJECT_NAME}_VERSION ${PROJECT_VERSION})
    set(${UPPER_PROJECT_NAME}_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
    set(${UPPER_PROJECT_NAME}_VERSION_MINOR ${PROJECT_VERSION_MINOR})
    set(${UPPER_PROJECT_NAME}_VERSION_PATCH ${PROJECT_VERSION_PATCH})
endmacro()

#
# setup directory related variables
#
macro(setup_dir_vars)
    set(OUTPUT_BINDIR ${PROJECT_BINARY_DIR}/bin)
    set(OUTPUT_LIBDIR ${PROJECT_BINARY_DIR}/lib)
    set(OUTPUT_DATADIR ${PROJECT_BINARY_DIR}/share)

    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_LIBDIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_BINDIR})

    include(GNUInstallDirs)

    if(WIN32)
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_BINDIR})
        # set up local bin directory to place all binaries
        make_directory(${OUTPUT_BINDIR})
        make_directory(${OUTPUT_LIBDIR})
    else()
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_LIBDIR})
        # set up local bin directory to place all binaries
        make_directory(${OUTPUT_LIBDIR})
    endif()
endmacro()

#
# create and install cmake support files
#
# available arguments:
#
#    CONFIG_TEMPLATE <file>   cmake template file for generating <prefix>Config.cmake file
#    [PREFIX <prefix>]        prefix for generating file names (optional)
#                             If not specified, ${PROJECT_NAME} is used
#    [VERSION <version>]      version used for <prefix>ConfigVersion.cmake (optional)
#                             If not specified, ${PROJECT_VERSION} is used
#
# The files maintained by this macro are
#
#    <prefix>Config.cmake
#    <prefix>ConfigVersion.cmake
#    <prefix>Targets*.cmake
#
macro(add_cmake_support_files)
    set(options)
    set(oneValueArgs CONFIG_TEMPLATE PREFIX)
    set(multiValueArgs)
    cmake_parse_arguments(ARGS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARGS_PREFIX)
        set(ARGS_PREFIX ${PROJECT_NAME})
    endif()
    if(NOT ARGS_VERSION)
        set(ARGS_VERSION ${PROJECT_VERSION})
    endif()
    set(CONFIG_FILE ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_PREFIX}Config.cmake)
    set(CONFIG_VERSION_FILE ${CMAKE_CURRENT_BINARY_DIR}/${ARGS_PREFIX}ConfigVersion.cmake)

    if(NOT ARGS_CONFIG_TEMPLATE)
        message(FATAL_ERROR "no template for generating <prefix>Config.cmake provided - use argument CONFIG_TEMPLATE <file>")
    endif()
    configure_file(${ARGS_CONFIG_TEMPLATE} ${CONFIG_FILE} @ONLY)

    install(EXPORT ${ARGS_PREFIX}Targets
        FILE ${ARGS_PREFIX}Targets.cmake
        NAMESPACE ${ARGS_PREFIX}::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${ARGS_PREFIX}
    )

    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
        ${CONFIG_VERSION_FILE}
        COMPATIBILITY SameMajorVersion
        VERSION ${ARGS_VERSION}
    )

    install(
        FILES
            ${CONFIG_FILE}
            ${CONFIG_VERSION_FILE}
        DESTINATION
            ${CMAKE_INSTALL_LIBDIR}/cmake/${ARGS_PREFIX}
    )
endmacro()

#
# add 'docs' build target
#
# available arguments:
#
#    FILES      list with file or directory names
#
macro(add_target_docs)
    set(options)
    set(oneValueArgs )
    set(multiValueArgs FILES)
    cmake_parse_arguments(ARGS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # create doxygen build target
    find_package(Doxygen QUIET)
    if (DOXYGEN_FOUND)
        set(DOXYGEN_GENERATE_HTML YES)
        set(DOXYGEN_GENERATE_MAN NO)

        doxygen_add_docs(
            docs
            ${ARGS_FILES}
            COMMENT "Use doxygen to Generate html documentation"
        )
        set_target_properties(docs PROPERTIES FOLDER ${PROJECT_NAME})
    endif()
endmacro()

#
# add 'uninstall' build target
#
macro(add_target_uninstall)
    # we are running inside VulkanSceneGraph
    if (PROJECT_NAME STREQUAL "vsg")
        # install file for client packages
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/cmake/uninstall.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vsg)
        set(DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
    else()
        set(DIR ${CMAKE_CURRENT_LIST_DIR})
    endif()
    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${DIR}/uninstall.cmake
    )
    set_target_properties(uninstall PROPERTIES FOLDER ${PROJECT_NAME})
endmacro()


#
# add options for vsg and all packages depending on vsg
#
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

