if(ROCKY_RENDERER_VSG)
    add_subdirectory(rsimple)
    add_subdirectory(rengine)

    if(ROCKY_SUPPORTS_IMGUI)
        add_subdirectory(rdemo)
    endif()
    
    if(ROCKY_SUPPORTS_QT)
        add_subdirectory(rdemoqt)
    endif()
endif()


