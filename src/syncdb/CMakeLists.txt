project(syncdb)

add_executable(syncdb main.c)
target_link_libraries(syncdb PRIVATE sqlite3)
