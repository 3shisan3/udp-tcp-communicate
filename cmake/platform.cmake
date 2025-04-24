if(WIN32)
    add_definitions(-D_WIN32)
    list(APPEND DEPEND_LIBS ws2_32)
endif()