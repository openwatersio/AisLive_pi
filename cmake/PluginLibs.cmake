# ~~~
# Summary:      Find and link general plugin libraries
# License:      GPLv3+
# Copyright (c) 2021 Alec Leamas
#
# Find and link general libraries to use: gettext, wxWidgets, OpenGL,
# and the vendored IXWebSocket (TLS via mbedtls, replaces OpenSSL).
# ~~~

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

find_package(Gettext REQUIRED)

#
# Windows environment check
#
set(_bad_win_env_msg [=[
%WXWIN% is not present in environment, win_deps.bat has not been run.
Build might work, but most likely fail when not finding wxWidgets.
Run buildwin\win_deps.bat or set %WXWIN% to mute this message.
]=])

if (WIN32 AND NOT DEFINED ENV{WXWIN})
  message(WARNING ${_bad_win_env_msg})
endif ()

#
# OpenGL
#
# Prefer libGL.so to libOpenGL.so, see CMP0072
set(OpenGL_GL_PREFERENCE "LEGACY")

find_package(OpenGL)
if (TARGET OpenGL::GL)
  target_link_libraries(${PACKAGE_NAME} OpenGL::GL)
else ()
  message(WARNING "Cannot locate usable OpenGL libs and headers.")
endif ()
if (NOT OPENGL_GLU_FOUND)
  message(WARNING "Cannot find OpenGL GLU extension.")
endif ()
if (APPLE)
  # As of 3.19.2, cmake's FindOpenGL does not link to the directory
  # containing gl.h. cmake bug? Intended due to missing subdir GL/gl.h?
  find_path(GL_H_DIR NAMES gl.h)
  if (GL_H_DIR)
    target_include_directories(${PACKAGE_NAME} PRIVATE "${GL_H_DIR}")
  else ()
    message(WARNING "Cannot locate OpenGL header file gl.h")
  endif ()
endif ()
if (WIN32)
  if (EXISTS "${PROJECT_SOURCE_DIR}/opencpn-libs/WindowsHeaders")
    add_subdirectory("${PROJECT_SOURCE_DIR}/opencpn-libs/WindowsHeaders")
    target_link_libraries(${PACKAGE_NAME} windows::headers)
  else ()
    message(STATUS
      "WARNING: WindowsHeaders library is missing, OpenGL unavailable"
    )
  endif ()
endif ()

#
# wxWidgets
#
set(wxWidgets_USE_DEBUG OFF)
set(wxWidgets_USE_UNICODE ON)
set(wxWidgets_USE_UNIVERSAL OFF)
set(wxWidgets_USE_STATIC OFF)

set(WX_COMPONENTS base core net xml html adv stc aui)
if (TARGET OpenGL::OpenGL OR TARGET OpenGL::GL)
  list(APPEND WX_COMPONENTS gl)
endif ()

find_package(wxWidgets REQUIRED ${WX_COMPONENTS})
include(${wxWidgets_USE_FILE})
target_link_libraries(${PACKAGE_NAME} ${wxWidgets_LIBRARIES})

#
# --- BEGIN CHANGED SECTION: OpenSSL -> vendored IXWebSocket -------------
# Previously this file called find_package(OpenSSL REQUIRED) and linked
# OpenSSL::SSL / OpenSSL::Crypto. We now vendor IXWebSocket from
# third-party/IXWebSocket and let it bring its own TLS backend (mbedtls,
# also vendored) -- no system OpenSSL is required on the user's PC.
#
# Cache variable names and the static-link / PIC dance are taken verbatim
# from opencpn-radar-pi/mayara_pi (top-level CMakeLists.txt, "mayara-server
# client networking" block) so the two plugins stay in sync.
# AisLive talks to wss://aisstream.io so TLS must stay ON; the choice
# between USE_MBED_TLS / USE_OPEN_SSL / USE_SECURE_TRANSPORT / USE_LIBRE_SSL
# is left to the mayara_pi convention by setting USE_MBED_TLS=ON (no
# preinstalled dependency on any host platform).
#
# AisLive_pi TLS backend (vendored IXWebSocket + vendored mbedtls):
#   USE_TLS         = ON     (wss://aisstream.io requires TLS)
#   USE_MBED_TLS    = ON     (vendored, zero preinstalled deps)
#   USE_OPEN_SSL    = unset  (would re-introduce the system OpenSSL dep
#                             we are removing; explicitly NOT set here)
#   USE_ZLIB        = ON     (permessage-deflate; OFF on WIN32 because
#                             MSVC has no system zlib find_package works
#                             for, mirroring mayara_pi)
#   IXWEBSOCKET_INSTALL = OFF (we never install ixwebsocket; it is private)
#   BUILD_SHARED_LIBS   = OFF (force static; PIC so it links into the
#                             plugin shared object; FE2 sets BUILD_SHARED_LIBS
#                             globally so we save/restore it around
#                             add_subdirectory, exactly like mayara_pi)
# ------------------------------------------------------------------------
set(USE_TLS ON CACHE BOOL "" FORCE)
set(USE_MBED_TLS ON CACHE BOOL "" FORCE)
# zlib enables permessage-deflate; it's a system library on macOS/Linux but
# not on Windows/MSVC (find_package(ZLIB) would fail the configure); drop it
# there. WebSocket compression is negotiated, so uncompressed frames still
# work -- just a little more bandwidth on Windows.
if (WIN32)
  set(USE_ZLIB OFF CACHE BOOL "" FORCE)
else ()
  set(USE_ZLIB ON CACHE BOOL "" FORCE)
endif ()
set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)
# Build IXWebSocket as a STATIC lib linked into the plugin (FE2 sets
# BUILD_SHARED_LIBS globally, which would otherwise emit a separate dylib the
# plugin can't find at load time). PIC so it links into our shared object.
set(_aislive_saved_shared ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_subdirectory(${PROJECT_SOURCE_DIR}/third-party/IXWebSocket)
set(BUILD_SHARED_LIBS ${_aislive_saved_shared})
target_link_libraries(${PACKAGE_NAME} ixwebsocket)
target_include_directories(
  ${PACKAGE_NAME}
  PRIVATE
    ${PROJECT_SOURCE_DIR}/third-party/IXWebSocket
)
# --- END CHANGED SECTION: OpenSSL -> vendored IXWebSocket ---------------
