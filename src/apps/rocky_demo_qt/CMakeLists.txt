set(APP_NAME rocky_demo_qt)

find_package(vsgQt CONFIG REQUIRED)

if(vsgQt_FOUND)
    find_package(vsg CONFIG REQUIRED)
    find_package(glslang CONFIG REQUIRED)
    find_package(Qt5 COMPONENTS Widgets CONFIG REQUIRED)

    file(GLOB SOURCES *.cpp)
    file(GLOB HEADERS *.h)

    # enable folders for IDEs
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    assign_source_groups("Source Files" "${CMAKE_CURRENT_SOURCE_DIR}" ${SOURCES})
    assign_source_groups("Header Files" "${CMAKE_CURRENT_SOURCE_DIR}" ${HEADERS})
    
    message(STATUS "QT DIR = ${QT_DIR}")    
    include_directories(${QT_DIR}/qt5)

    add_executable(${APP_NAME} ${SOURCES} ${HEADERS})

    target_link_libraries(${APP_NAME} rocky vsgQt::vsgQt)

    install(TARGETS ${APP_NAME} RUNTIME DESTINATION bin)

    set_target_properties(${APP_NAME} PROPERTIES FOLDER "apps")
endif()
