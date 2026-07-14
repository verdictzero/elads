cmake_minimum_required(VERSION 3.16)

project(elads
    VERSION 0.0.1
    DESCRIPTION "A unified Doom/ZDoom/GZDoom development environment for Linux (Raspberry Pi 5 first)"
    HOMEPAGE_URL "https://github.com/verdictzero/elads"
    LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# elads is in the DESIGN phase. No feature source exists yet, so the default
# build is a "stub" configure that validates the scaffold (directory layout,
# presets, options) WITHOUT requiring wxWidgets / OpenGL / Lua. This lets the
# repository configure cleanly on any machine and in CI before code lands.
#
# When Phase 1 begins, flip ELADS_STUB=OFF and the real dependency discovery
# below (currently a documented placeholder) becomes active.
# ---------------------------------------------------------------------------

option(ELADS_STUB "Configure the scaffold only; skip feature build + dependency discovery" ON)
option(ELADS_CORE "Build the GUI/GL-free core library (util, archive, map model, render interface) + tests" OFF)
option(ELADS_GL "Also build the desktop OpenGL backend + headless renderer (needs EGL + libepoxy). Implies ELADS_CORE." OFF)

# Render-backend selection (see docs/design/02-render-abstraction.md).
# GLES 3.1 is the primary, Pi-appropriate target. Others are fallbacks.
set(ELADS_RENDER_BACKEND "gles" CACHE STRING "Render backend: gles | desktopgl | zink")
set_property(CACHE ELADS_RENDER_BACKEND PROPERTY STRINGS gles desktopgl zink)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif()

# ELADS_GL implies ELADS_CORE (the GL renderer builds on top of the core).
if(ELADS_GL)
    set(ELADS_CORE ON)
endif()

message(STATUS "elads ${PROJECT_VERSION}")
message(STATUS "  ELADS_STUB           = ${ELADS_STUB}")
message(STATUS "  ELADS_CORE           = ${ELADS_CORE}")
message(STATUS "  ELADS_GL             = ${ELADS_GL}")
message(STATUS "  ELADS_RENDER_BACKEND = ${ELADS_RENDER_BACKEND}")
message(STATUS "  CMAKE_SYSTEM_PROCESSOR = ${CMAKE_SYSTEM_PROCESSOR}")

# CORE mode: build the GUI/GL-free core library + tests. This needs no wxWidgets/OpenGL,
# so it compiles and runs on any C++17 toolchain (incl. CI x86 and native aarch64).
if(ELADS_CORE)
    message(STATUS "")
    message(STATUS "elads: CORE mode — building the GUI/GL-free core library + tests.")
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    add_subdirectory(src)
    enable_testing()
    add_subdirectory(tests)
    return()
endif()

if(ELADS_STUB)
    message(STATUS "")
    message(STATUS "elads: STUB configure — validating scaffold only, no targets built.")
    message(STATUS "elads: this is expected in the design phase. See docs/roadmap.md.")

    # A tiny target so `cmake --build` succeeds and CI has something to run.
    add_custom_target(elads_scaffold_check ALL
        COMMAND ${CMAKE_COMMAND} -E echo "elads scaffold OK (stub mode); no feature code yet."
        VERBATIM)
    return()
endif()

# ---------------------------------------------------------------------------
# REAL BUILD (ELADS_STUB=OFF) — placeholder for Phase 1.
# The dependency set (from docs/design/*) will be, on Linux:
#   wxWidgets 3.2.9+ (std aui gl stc richtext propgrid), OpenGL ES / EGL,
#   Lua 5.4 + sol2, SFML (audio), FreeType/FTGL, libpng, libwebp, zlib, bzip2,
#   earcut.hpp (vendored), Scintilla/Lexilla (vendored).
# ---------------------------------------------------------------------------
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message(WARNING
    "elads: ELADS_STUB=OFF but no feature sources are present yet (pre-alpha).\n"
    "       Dependency discovery and src/ targets will be added in Phase 1.\n"
    "       See docs/roadmap.md and docs/design/01-architecture.md.")

# Example of the discovery that will live here (kept commented until code lands):
#   find_package(wxWidgets 3.2 REQUIRED COMPONENTS std aui gl stc richtext propgrid)
#   find_package(OpenGL REQUIRED COMPONENTS OpenGL EGL)   # EGL for Wayland
#   find_package(Lua 5.4 REQUIRED)
#   include(cmake/thirdparty.cmake)
#   add_subdirectory(src)
#   enable_testing()
#   add_subdirectory(tests)
