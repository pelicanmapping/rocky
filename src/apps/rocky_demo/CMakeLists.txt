set(APP_NAME rocky_demo)

if(USING_VCPKG)
    find_package(imgui CONFIG)
    set(MY_IMGUI_LINK_LIBRARIES imgui::imgui)
else()
    include(FindPkgConfig)
    find_package(imgui)
    pkg_check_modules(imgui imgui)
    set(MY_IMGUI_LINK_LIBRARIES ${imgui_LIBRARIES})
    set(temp "")
    foreach(dir ${imgui_INCLUDE_DIRS})
        list(APPEND temp "${dir}")
        list(APPEND temp "${dir}/backends")
    endforeach()
    set(imgui_INCLUDE_DIRS ${temp})
    message(STATUS "IMGUI_LIBRARIES = ${imgui_LIBRARIES}")
    message(STATUS "IMGUI_INCLUDE_DIRS = ${imgui_INCLUDE_DIRS}")
endif()

if(imgui_FOUND)
    find_package(glslang CONFIG REQUIRED)

    file(GLOB SOURCES *.cpp vsgImGui/*.cpp)
    file(GLOB HEADERS *.h vsgImGui/*.h)

    # enable folders for IDEs
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    assign_source_groups("Source Files" "${CMAKE_CURRENT_SOURCE_DIR}" ${SOURCES})
    assign_source_groups("Header Files" "${CMAKE_CURRENT_SOURCE_DIR}" ${HEADERS})

    add_executable(${APP_NAME} ${SOURCES} ${HEADERS})
    
    target_include_directories(${APP_NAME} PRIVATE ${imgui_INCLUDE_DIRS})
    target_link_libraries(${APP_NAME} rocky ${MY_IMGUI_LINK_LIBRARIES})

    install(TARGETS ${APP_NAME} RUNTIME DESTINATION bin)

    set_target_properties(${APP_NAME} PROPERTIES FOLDER "apps")
else()
    message(WARN "imgui not found, ${APP_NAME} will not be built :(")
endif()
