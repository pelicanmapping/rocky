set(APP_NAME rocky_sandbox)

file(GLOB SOURCES *.cpp)
file(GLOB SHADERS *.vert *.frag)

add_executable(${APP_NAME} ${SOURCES} ${SHADERS})

target_link_libraries(${APP_NAME} rocky)

install(TARGETS ${APP_NAME} RUNTIME DESTINATION bin)
install(FILES ${SHADERS} DESTINATION share/rocky/shaders)

set_target_properties(${APP_NAME} PROPERTIES FOLDER "apps")
