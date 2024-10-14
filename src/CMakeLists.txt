set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# sort into group folders to help IDEs present the files in a logical manner
function(assign_source_groups GROUP_NAME ROOT_FOLDER)
    foreach(FILE IN ITEMS ${ARGN})
        if (IS_ABSOLUTE "${FILE}")
            file(RELATIVE_PATH RELATIVE_SOURCE "${ROOT_FOLDER}" "${FILE}")
        else()
            set(RELATIVE_SOURCE "${FILE}")
        endif()
        get_filename_component(SOURCE_PATH "${RELATIVE_SOURCE}" PATH)
        string(REPLACE "/" "\\" SOURCE_PATH_MSVC "${SOURCE_PATH}")
        source_group("${GROUP_NAME}\\${SOURCE_PATH_MSVC}" FILES "${FILE}")
    endforeach()
endfunction()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# For windows import/export
if(NOT ROCKY_BUILD_SHARED_LIBS)
    add_definitions(-DROCKY_STATIC)
endif()

add_subdirectory(rocky)
add_subdirectory(apps)
add_subdirectory(tests)
