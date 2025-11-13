set(APP_NAME rocky_demo_qt)

find_package(vsg CONFIG REQUIRED)
find_package(glslang CONFIG REQUIRED)
find_package(vsgQt CONFIG REQUIRED)

# Find Qt, if vsgQt didn't already find it
if(NOT TARGET Qt::Widgets)
    # Figure out what version of Qt vsgQt linked against by inspecting dependencies. Match vsgQt.
    get_target_property(_VSGQT_INTERFACE_LINK_LIBRARIES vsgQt::vsgQt INTERFACE_LINK_LIBRARIES)
    set(_ROCKY_QT_VERSION 6)
    if("${_VSGQT_INTERFACE_LINK_LIBRARIES}" MATCHES "Qt5::")
        set(_ROCKY_QT_VERSION 5)
    endif()
    find_package(Qt${_ROCKY_QT_VERSION} REQUIRED COMPONENTS Widgets CONFIG)
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
