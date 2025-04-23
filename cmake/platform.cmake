if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE -D_WIN32) 
    list(APPEND DEPEND_LIBS ws2_32)
endif()