message(STATUS "Welcome to rengine")

file(GLOB SOURCES *.cpp)

add_executable(rengine ${SOURCES})

target_link_libraries(rengine rocky rocky_vsg)

install(TARGETS rengine RUNTIME DESTINATION bin)

set_target_properties(rengine PROPERTIES FOLDER "apps")
