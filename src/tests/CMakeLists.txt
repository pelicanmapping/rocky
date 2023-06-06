
set(SOURCES
    tests.cpp
    catch.hpp
)

add_executable(rtests ${SOURCES})

target_link_libraries(rtests rocky nlohmann_json::nlohmann_json)

install(TARGETS rtests RUNTIME DESTINATION bin)

set_target_properties(rtests PROPERTIES FOLDER "tests")
