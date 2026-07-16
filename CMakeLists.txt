cmake_minimum_required(VERSION 3.21)

project(hardlinkmgr
        VERSION 0.1.0
        DESCRIPTION "GUI tool for manual SMB hard-link deduplication"
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Stage all executables in one place so Qt's runtime DLLs are easy to deploy
# alongside them later (windeployqt / macdeployqt).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------------------
# libsmb2 — patched fork carrying the smb2_link hard-link patch (see ADR 0001
# and spikes/01_libsmb2). Pinned to an exact commit: bump this deliberately and
# re-verify the patch whenever libsmb2 is updated.
# ---------------------------------------------------------------------------
include(FetchContent)

# Configure libsmb2 before it is added. Use CACHE ... FORCE rather than plain
# set(): libsmb2's CMakeLists calls cmake_minimum_required(VERSION 3.5), which
# resets CMP0077 to OLD inside its scope, so its option() would otherwise ignore
# a normal variable and default BUILD_SHARED_LIBS back to ON.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build libsmb2 statically" FORCE) # static: nothing to stage at runtime
set(ENABLE_EXAMPLES   OFF CACHE BOOL "" FORCE)   # we don't need its sample programs
set(ENABLE_LIBKRB5    OFF CACHE BOOL "" FORCE)   # app authenticates via NTLMSSP; no Kerberos
set(ENABLE_GSSAPI     OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  libsmb2
  GIT_REPOSITORY https://github.com/danbrakeley/libsmb2.git
  GIT_TAG        858184e837180410906ce34e8a56984178f1e55a
  GIT_SHALLOW    FALSE   # required: a shallow clone can't check out an arbitrary SHA
)
FetchContent_MakeAvailable(libsmb2)

# ---------------------------------------------------------------------------
# Qt — found from the installed kit. CMAKE_PREFIX_PATH is supplied by the
# active CMake preset (see CMakePresets.json).
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

qt_add_executable(hardlinkmgr
  src/main.cpp
)

# Note: intentionally a console app for now (stdout visible during early dev).
# Flip to a GUI app later with: set_target_properties(hardlinkmgr PROPERTIES WIN32_EXECUTABLE ON)
target_link_libraries(hardlinkmgr PRIVATE
  Qt6::Widgets
  smb2
)

# libsmb2's public headers include <winsock2.h>/<windows.h>; without
# WIN32_LEAN_AND_MEAN that drags in the COM headers, which #define `interface`
# and collide with a struct member of that name in libsmb2's dcerpc header.
# This is required for any TU that includes libsmb2 on Windows.
if(WIN32)
  target_compile_definitions(hardlinkmgr PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

  # Stage Qt's runtime DLLs (and plugins) next to the exe after each build so it
  # runs / double-clicks without Qt on PATH. windeployqt auto-detects the build
  # config from the binary.
  add_custom_command(TARGET hardlinkmgr POST_BUILD
    COMMAND Qt6::windeployqt
            --no-translations
            --no-compiler-runtime
            "$<TARGET_FILE:hardlinkmgr>"
    COMMENT "Deploying Qt runtime dependencies next to hardlinkmgr.exe"
    VERBATIM)
endif()
