message(STATUS "Welcome to rviewer")

file(GLOB SOURCES *.cpp)

add_executable(rviewer ${SOURCES})

target_link_libraries(rviewer rocky rocky_vsg)

install(TARGETS rviewer RUNTIME DESTINATION bin)

set_target_properties(rviewer PROPERTIES FOLDER "apps")
