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

# treat [[nodisard]] as an error in VS
if (WIN32)
    add_compile_options(/we4834)
endif()

option(ROCKY_DEBUG_MEMCHECK "Instrument code for memcheck analysis and debugging" OFF)
mark_as_advanced(ROCKY_DEBUG_MEMCHECK)  

add_subdirectory(rocky)
add_subdirectory(apps)
add_subdirectory(tests)
