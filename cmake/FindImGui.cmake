# Finds the ImGui library and creates the target imgui::imgui
#
# Inputs:
#    IMGUI_DIR : Set this to compile imgui from source found in the folder
#
# Outputs:
#    ImGui_FOUND : True if everything worked out and imgui::imgui is available.
#

if (NOT TARGET imgui::imgui)

    # Check for a CONFIG file first (vcpkg uses one):
    # vcpkg names the library "imgui::imgui" so we'll follow that pattern here.
    if(NOT IMGUI_DIR)

        find_package(imgui CONFIG QUIET)
        if (TARGET imgui::imgui)
            set(ImGui_FOUND TRUE)
        endif()

    else()
    
        message(STATUS "Since you set IMGUI_DIR, I will try to build ImGui from source.")
        
        set(IMGUI_SOURCES "")

        # find the imgui sources based on the IMGUI_DIR variable the user passed in:
        find_path(IMGUI_INCLUDE_DIR NAMES imgui.h
            HINTS ${IMGUI_DIR}
            NO_CACHE NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
        mark_as_advanced(IMGUI_INCLUDE_DIR)

        if (IMGUI_INCLUDE_DIR)       
            set(IMGUI_ALL_SOURCES_FOUND TRUE)
        
            add_library(imgui::imgui INTERFACE IMPORTED)
            
            set_property(TARGET imgui::imgui APPEND PROPERTY
                INTERFACE_INCLUDE_DIRECTORIES ${IMGUI_INCLUDE_DIR} "${IMGUI_INCLUDE_DIR}/backends")
            
            foreach(_file imgui.cpp imgui_widgets.cpp imgui_draw.cpp imgui_tables.cpp backends/imgui_impl_vulkan.cpp)            
                find_file(_source_${_file} NAMES ${_file}
                    HINTS ${IMGUI_DIR}
                    NO_CACHE NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH )

                if(NOT _source_${_file})
                    set(IMGUI_ALL_SOURCES_FOUND FALSE)          
                    message(WARNING "Failed to find ${_source_${_file}} in ${IMGUI_DIR}")
                    break()
                endif()

                list(APPEND IMGUI_SOURCES ${_source_${_file}})
                
                unset(_source_${_file})
            endforeach()

            if(IMGUI_ALL_SOURCES_FOUND)                
                add_library(imgui::sources INTERFACE IMPORTED)
                
                set_property(TARGET imgui::sources APPEND PROPERTY
                    INTERFACE_SOURCES "${IMGUI_SOURCES}")
                    
                # make imgui depend on sources:
                set_property(TARGET imgui::imgui APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES imgui::sources)          
            endif()
        else()
            message(WARNING "Could not find any ImGui source code at ${IMGUI_DIR}")
        endif()
        
        if(IMGUI_ALL_SOURCES_FOUND)
            set(ImGui_FOUND TRUE)
        endif()
        
        unset(IMGUI_SOURCES)
        unset(IMGUI_ALL_SOURCES_FOUND)
    endif()
endif()

if(NOT ImGui_FOUND)
    message(WARNING "ImGui wasn't found. You may want to try setting IMGUI_DIR to automatically build it from source.")
endif()
