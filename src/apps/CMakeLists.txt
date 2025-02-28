if(ROCKY_RENDERER_VSG)
    add_subdirectory(rocky_simple)
    add_subdirectory(rocky_engine)

    if(ROCKY_SUPPORTS_IMGUI)
        add_subdirectory(rocky_demo)
    endif()
    
    if(ROCKY_SUPPORTS_QT)
        add_subdirectory(rocky_demo_qt)
    endif()
endif()
