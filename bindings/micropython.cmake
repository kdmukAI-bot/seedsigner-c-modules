add_library(usermod_dm INTERFACE)

target_sources(usermod_dm INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/moddisplay_manager_bindings.c
    ${CMAKE_CURRENT_LIST_DIR}/modseedsigner_bindings.c
)

target_include_directories(usermod_dm INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../components/display_manager
    ${CMAKE_CURRENT_LIST_DIR}/../components/esp_bsp
    ${CMAKE_CURRENT_LIST_DIR}/../components/esp_lv_port/include
    ${CMAKE_CURRENT_LIST_DIR}/../components/seedsigner
)

# Link bindings against ESP-IDF component libs instead of compiling component C++
# sources in usermod qstr extraction.
target_link_libraries(usermod_dm INTERFACE
    __idf_display_manager
    __idf_seedsigner
)

target_link_libraries(usermod INTERFACE usermod_dm)
