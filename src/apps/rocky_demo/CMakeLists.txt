set(APP_NAME rocky_demo)

find_package(ImGui REQUIRED)

file(GLOB SOURCES *.cpp)
file(GLOB HEADERS *.h)

# enable folders for IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
assign_source_groups("Source Files" "${CMAKE_CURRENT_SOURCE_DIR}" ${SOURCES})
assign_source_groups("Header Files" "${CMAKE_CURRENT_SOURCE_DIR}" ${HEADERS})

add_executable(${APP_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${APP_NAME} rocky imgui::imgui)

install(TARGETS ${APP_NAME} RUNTIME DESTINATION bin)

set_target_properties(${APP_NAME} PROPERTIES FOLDER "apps")

# MSVC requires /bigobj on debug mode
if(MSVC)
    target_compile_options(${APP_NAME} PRIVATE "$<$<CONFIG:Debug>:/bigobj>")
endif()
