# elads core tests — one executable per file, registered with CTest.
# Run with:  ctest --preset core   (or  ctest --test-dir build/core --output-on-failure)

set(ELADS_TESTS
    test_wad
    test_map_model
    test_render_interface
    test_doom_map_io
    test_udmf
    test_doom_gfx
    test_texturex
    test_sector_tri
    test_flat
    test_composite
    test_entry_type
    test_pk3
    test_map_checks
)

foreach(t ${ELADS_TESTS})
    add_executable(${t} ${t}.cpp)
    target_link_libraries(${t} PRIVATE elads_core)
    target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}) # for check.h
    add_test(NAME ${t} COMMAND ${t})
endforeach()
