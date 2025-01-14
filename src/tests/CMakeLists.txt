set(APP_NAME rocky_tests)

set(SOURCES tests.cpp catch.hpp)

add_executable(${APP_NAME} ${SOURCES})

target_link_libraries(${APP_NAME} rocky)

# Tests use json.h, which relies on nlohmann_json, which is not a public dependency of rocky
if (BUILD_WITH_JSON)
    find_package(nlohmann_json CONFIG)
    if (nlohmann_json_FOUND)
        target_link_libraries(${APP_NAME} nlohmann_json::nlohmann_json)
    endif()
endif()


install(TARGETS ${APP_NAME} RUNTIME DESTINATION bin)

set_target_properties(${APP_NAME} PROPERTIES FOLDER "tests")
