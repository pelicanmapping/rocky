set(APP_NAME rocky_tests)

set(SOURCES tests.cpp catch.hpp)

add_executable(${APP_NAME} ${SOURCES})

target_link_libraries(${APP_NAME} rocky)

install(TARGETS ${APP_NAME} RUNTIME DESTINATION bin)

set_target_properties(${APP_NAME} PROPERTIES FOLDER "tests")
