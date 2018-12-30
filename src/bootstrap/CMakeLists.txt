project(bootstrap)

add_executable(bootstrap main.c ini.c ini.h cfg.c cfg.h cdrom.c cdrom.h cover.c cover.h games.c games.h games_cache.c games_cache.h scanner.c scanner.h infodb.c infodb.h gamedb.c gamedb.h prep.c prep.h)
target_link_libraries(bootstrap PRIVATE sqlite3 miniz)

if(MSVC)
    target_compile_definitions(bootstrap PRIVATE -D_CRT_SECURE_NO_WARNINGS)
elseif(WIN32)
    target_link_options(bootstrap PRIVATE -static -static-libgcc)
endif()
target_link_options(bootstrap PRIVATE -Wl,-rpath,/usr/sony/lib)
