set(APP_NAME rocky_demo)

find_package(ImGui)

if(ImGui_FOUND)
    find_package(glslang CONFIG REQUIRED)

    file(GLOB SOURCES *.cpp vsgImGui/*.cpp)
    file(GLOB HEADERS *.h vsgImGui/*.h)

    # enable folders for IDEs
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    assign_source_groups("Source Files" "${CMAKE_CURRENT_SOURCE_DIR}" ${SOURCES})
    assign_source_groups("Header Files" "${CMAKE_CURRENT_SOURCE_DIR}" ${HEADERS})

    add_executable(${APP_NAME} ${SOURCES} ${HEADERS})
    
    target_link_libraries(${APP_NAME} rocky imgui::imgui)

    install(TARGETS ${APP_NAME} RUNTIME DESTINATION bin)

    set_target_properties(${APP_NAME} PROPERTIES FOLDER "apps")
else()
    message(WARNING "imgui not found, ${APP_NAME} will not be built :(")
endif()
