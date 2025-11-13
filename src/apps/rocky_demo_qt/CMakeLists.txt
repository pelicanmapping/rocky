set(APP_NAME rocky_demo_qt)

find_package(vsg CONFIG REQUIRED)
find_package(glslang CONFIG REQUIRED)
find_package(vsgQt CONFIG REQUIRED)

# Find Qt, if vsgQt didn't already find it
if(NOT TARGET Qt::Widgets)
    find_package(Qt6 QUIET COMPONENTS Widgets CONFIG)
    if(Qt6_FOUND)
        message(STATUS "Found Qt6: ${Qt6_DIR}")
    else()
        find_package(Qt5 QUIET COMPONENTS Widgets CONFIG)
        if(Qt5_FOUND)
            message(STATUS "Found Qt5: ${Qt5_DIR}")
            include_directories(${Qt5_DIR}/qt5)
        else()
            # Provide a warning if neither is found, but don't stop the configuration.
            message(WARNING "Neither Qt5 nor Qt6 was found. rocky_demo_qt will not build.")
            return()
        endif()
    endif()
endif()

file(GLOB SOURCES *.cpp)
file(GLOB HEADERS *.h)

# enable folders for IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
assign_source_groups("Source Files" "${CMAKE_CURRENT_SOURCE_DIR}" ${SOURCES})
assign_source_groups("Header Files" "${CMAKE_CURRENT_SOURCE_DIR}" ${HEADERS})
    
add_executable(${APP_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${APP_NAME} rocky vsgQt::vsgQt)

install(TARGETS ${APP_NAME} RUNTIME DESTINATION bin)

set_target_properties(${APP_NAME} PROPERTIES FOLDER "apps")
