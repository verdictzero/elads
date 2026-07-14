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
    test_slopes
    test_tex_align
    test_pick
    test_map_edit
    test_map_save
    test_editor
    test_billboard
    test_sector_style
    test_flat
    test_composite
    test_entry_type
    test_pk3
    test_map_checks
    test_png
    test_wad_materials
)

foreach(t ${ELADS_TESTS})
    add_executable(${t} ${t}.cpp)
    target_link_libraries(${t} PRIVATE elads_core)
    target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}) # for check.h
    add_test(NAME ${t} COMMAND ${t})
endforeach()

# GL render tests — only when the desktop OpenGL backend is built.
if(ELADS_GL)
    foreach(t test_gl_render test_gl_render3d test_gl_overlay test_gl_things3d test_gl_sectorcolor test_gl_fog)
        add_executable(${t} ${t}.cpp)
        target_link_libraries(${t} PRIVATE elads_render)
        target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME ${t} COMMAND ${t})
    endforeach()

    # Interactive-window test — needs GLFW. Self-skips when no display is present, so it is
    # safe to run headless; run it under xvfb-run to exercise the real window path.
    if(TARGET elads_window)
        add_executable(test_view_window test_view_window.cpp)
        target_link_libraries(test_view_window PRIVATE elads_window)
        target_include_directories(test_view_window PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        add_test(NAME test_view_window COMMAND test_view_window)
    endif()
endif()
