message(STATUS "Welcome to rviewer")

set(SOURCES
    rviewer.cpp
)

# find_package(rocky)

add_executable(rviewer ${SOURCES})

target_link_libraries(rviewer rocky rocky_vsg)

# if (vsgXchange_FOUND)
#     target_compile_definitions(rviewer PRIVATE vsgXchange_FOUND)
#     target_link_libraries(rviewer vsgXchange::vsgXchange)
# endif()

install(TARGETS rviewer RUNTIME DESTINATION bin)

set_target_properties(rviewer PROPERTIES FOLDER "apps")
