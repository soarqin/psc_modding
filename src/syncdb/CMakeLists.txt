project(syncdb)

add_executable(syncdb main.c ini.c ini.h)
target_link_libraries(syncdb PRIVATE sqlite3)

if(MSVC)
    target_compile_definitions(syncdb PRIVATE -D_CRT_SECURE_NO_WARNINGS)
endif()