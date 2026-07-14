# elads core library — GUI/GL-free foundation (no wxWidgets, no OpenGL).
#
# Header-only modules (util/, render/backend/, mapeditor/model/*.h) are consumed via the
# PUBLIC include directory; only the .cpp translation units are listed here.

add_library(elads_core STATIC
    archive/wad.cpp
    archive/entry_type.cpp
    mapeditor/model/map_model.cpp
    mapeditor/model/doom_map_io.cpp
    mapeditor/model/udmf.cpp
    mapeditor/model/sector_tri.cpp
    graphics/palette.cpp
    graphics/doom_gfx.cpp
    graphics/texturex.cpp
    graphics/flat.cpp
    graphics/composite.cpp
)

target_include_directories(elads_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# Vendored header-only earcut (ADR-0008), used by sector_tri.cpp. SYSTEM so its warnings
# don't surface under -Wall -Wextra.
target_include_directories(elads_core SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party/earcut)
target_compile_features(elads_core PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(elads_core PRIVATE /W4)
else()
    target_compile_options(elads_core PRIVATE -Wall -Wextra)
endif()

# elads — command-line front-end over the core (no GUI/GL).
add_executable(elads app/cli_main.cpp)
target_link_libraries(elads PRIVATE elads_core)
