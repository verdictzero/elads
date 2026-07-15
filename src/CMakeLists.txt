# elads core library — GUI/GL-free foundation (no wxWidgets, no OpenGL).
#
# Header-only modules (util/, render/backend/, mapeditor/model/*.h) are consumed via the
# PUBLIC include directory; only the .cpp translation units are listed here.

# Vendored miniz (public domain) built as its own C library, without our strict warnings.
add_library(miniz STATIC
    ${CMAKE_SOURCE_DIR}/third_party/miniz/miniz.c
    ${CMAKE_SOURCE_DIR}/third_party/miniz/miniz_tinfl.c
    ${CMAKE_SOURCE_DIR}/third_party/miniz/miniz_tdef.c
    ${CMAKE_SOURCE_DIR}/third_party/miniz/miniz_zip.c
)
target_include_directories(miniz SYSTEM PUBLIC ${CMAKE_SOURCE_DIR}/third_party/miniz)
set_target_properties(miniz PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(elads_core STATIC
    archive/wad.cpp
    archive/entry_type.cpp
    archive/pk3.cpp
    mapeditor/model/map_model.cpp
    mapeditor/model/doom_map_io.cpp
    mapeditor/model/udmf.cpp
    mapeditor/model/map_save.cpp
    mapeditor/model/sector_tri.cpp
    mapeditor/model/planes.cpp
    mapeditor/model/threed_floors.cpp
    mapeditor/model/map_checks.cpp
    mapeditor/edit/selection.cpp
    mapeditor/edit/map_edit.cpp
    mapeditor/edit/editor.cpp
    nodebuild/nodebuild.cpp
    graphics/palette.cpp
    graphics/doom_gfx.cpp
    graphics/texturex.cpp
    graphics/flat.cpp
    graphics/composite.cpp
    graphics/png.cpp
    graphics/wad_materials.cpp
    mapeditor/view2d/map_view_2d.cpp
    mapeditor/view3d/map_view_3d.cpp
)

target_include_directories(elads_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# Vendored header-only earcut (ADR-0008), used by sector_tri.cpp. SYSTEM so its warnings
# don't surface under -Wall -Wextra.
target_include_directories(elads_core SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party/earcut)
target_link_libraries(elads_core PRIVATE miniz) # pk3.cpp
target_compile_features(elads_core PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(elads_core PRIVATE /W4)
else()
    target_compile_options(elads_core PRIVATE -Wall -Wextra)
endif()

# elads — command-line front-end over the core (no GUI/GL).
add_executable(elads app/cli_main.cpp)
target_link_libraries(elads PRIVATE elads_core)

# --- Desktop OpenGL renderer (ELADS_GL) ---------------------------------------------------
if(ELADS_GL)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(EPOXY REQUIRED IMPORTED_TARGET epoxy)
    pkg_check_modules(EGL REQUIRED IMPORTED_TARGET egl)

    add_library(elads_render STATIC
        render/gl/egl_headless.cpp
        render/gl/gl_backend.cpp
        render/gl/offscreen.cpp
    )
    target_include_directories(elads_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(elads_render PUBLIC elads_core PkgConfig::EPOXY PkgConfig::EGL)
    target_compile_features(elads_render PUBLIC cxx_std_17)
    target_compile_options(elads_render PRIVATE -Wall -Wextra)

    # elads-render — headless map -> PNG tool.
    add_executable(elads-render app/render_main.cpp)
    target_link_libraries(elads-render PRIVATE elads_render)

    # --- Interactive window (elads-view), gated on GLFW being available ---------------------
    pkg_check_modules(GLFW IMPORTED_TARGET glfw3)
    if(GLFW_FOUND)
        add_library(elads_window STATIC render/gl/glfw_window.cpp)
        target_include_directories(elads_window PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
        target_link_libraries(elads_window PUBLIC elads_render PkgConfig::GLFW)
        target_compile_features(elads_window PUBLIC cxx_std_17)
        target_compile_options(elads_window PRIVATE -Wall -Wextra)

        add_executable(elads-view app/view_main.cpp)
        target_link_libraries(elads-view PRIVATE elads_window)
    else()
        message(STATUS "elads: GLFW not found — skipping the interactive elads-view target.")
    endif()
endif()
