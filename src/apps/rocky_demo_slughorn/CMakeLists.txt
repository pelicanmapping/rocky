set(APP_NAME rocky_demo_slughorn)

add_executable(${APP_NAME} rocky_demo_slughorn.cpp)

target_link_libraries(${APP_NAME} rocky)

install(TARGETS ${APP_NAME} RUNTIME DESTINATION bin)

set_target_properties(${APP_NAME} PROPERTIES FOLDER "apps")
