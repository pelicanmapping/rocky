set(APP_NAME rocky_engine)

file(GLOB SOURCES *.cpp)

add_executable(${APP_NAME} ${SOURCES})

target_link_libraries(${APP_NAME} rocky)

install(TARGETS ${APP_NAME} RUNTIME DESTINATION bin)

set_target_properties(${APP_NAME} PROPERTIES FOLDER "apps")
