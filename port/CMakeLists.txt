# port/CMakeLists.txt — standalone CamStreamEmu + arm64 .deb for the official
# CardputerZero AppStore. Build inside an arm64 Debian container (matches the
# device ABI). LVGL v9.5 via -DLVGL_DIR; GStreamer for the reproduction engine.
# See build.sh.
#
# Open-source components linked here: LVGL (MIT), FreeType (FTL/GPLv2),
# GStreamer 1.0 + gstreamer-app + gstreamer-rtsp-server (LGPLv2.1) and, at
# runtime, GStreamer plugins base/good/bad/libav (LGPL; gst-libav wraps FFmpeg
# and x264 which are GPL). See THIRD_PARTY_NOTICES.md.
cmake_minimum_required(VERSION 3.20)
project(camstreamemu VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(PORT_FBDEV "Build the on-device (fbdev + evdev) variant" OFF)
set(LVGL_DIR "" CACHE PATH "Path to an LVGL v9.5 source tree")
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

if(NOT LVGL_DIR OR NOT EXISTS "${LVGL_DIR}/lvgl.h")
    message(FATAL_ERROR "Set -DLVGL_DIR to an LVGL v9.5 source tree")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(FREETYPE REQUIRED freetype2)
pkg_check_modules(GST REQUIRED
    gstreamer-1.0 gstreamer-app-1.0 gstreamer-rtsp-server-1.0)

set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE FILEPATH "" FORCE)
add_subdirectory("${LVGL_DIR}" "${CMAKE_BINARY_DIR}/lvgl" EXCLUDE_FROM_ALL)
target_include_directories(lvgl PRIVATE ${FREETYPE_INCLUDE_DIRS})
target_link_libraries(lvgl PUBLIC ${FREETYPE_LIBRARIES})

# app UI + reproduction engine (repo-root .c files; NOT cli.c — that is the CLI)
add_executable(camstreamemu
    "${CMAKE_CURRENT_SOURCE_DIR}/main.c"
    "${REPO_ROOT}/src/app.c"
    "${REPO_ROOT}/profile.c"
    "${REPO_ROOT}/repro.c"
    "${REPO_ROOT}/video_pipe.c"
    "${REPO_ROOT}/tts_wrap.c"
    "${REPO_ROOT}/rtsp_serve.c"
    "${REPO_ROOT}/mjpeg_serve.c"
)
target_include_directories(camstreamemu PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}" "${REPO_ROOT}/emu"
    "${REPO_ROOT}/src" "${REPO_ROOT}"
    "${LVGL_DIR}" ${FREETYPE_INCLUDE_DIRS} ${GST_INCLUDE_DIRS})
target_compile_definitions(camstreamemu PRIVATE
    LV_CONF_INCLUDE_SIMPLE LV_LVGL_H_INCLUDE_SIMPLE)
target_link_libraries(camstreamemu PRIVATE
    lvgl ${FREETYPE_LIBRARIES} ${GST_LIBRARIES} pthread m)
target_link_directories(camstreamemu PRIVATE ${GST_LIBRARY_DIRS})
if(PORT_FBDEV)
    target_compile_definitions(camstreamemu PRIVATE PORT_FBDEV)
    target_sources(camstreamemu PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/evdev_kbd.c")
endif()

# ---- install layout: /usr/share/APPLaunch (Exec=bin/... is relative) ----
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/camstreamemu.desktop"
"[Desktop Entry]\nName=CamStreamEmu\nExec=bin/camstreamemu\nTerminal=false\nIcon=share/images/camstreamemu.png\nType=Application\n")

install(TARGETS camstreamemu RUNTIME DESTINATION share/APPLaunch/bin)
install(FILES "${REPO_ROOT}/share/images/camstreamemu.png"
        DESTINATION share/APPLaunch/share/images)
install(DIRECTORY "${REPO_ROOT}/samples/"
        DESTINATION share/APPLaunch/share/samples FILES_MATCHING PATTERN "*.json")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/camstreamemu.desktop"
        DESTINATION share/APPLaunch/applications)

# ---- CPack: arm64 .deb (Maintainer must equal the PR author at publish) ----
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_PACKAGE_NAME "camstreamemu")
set(CPACK_DEBIAN_PACKAGE_NAME "camstreamemu")
set(CPACK_DEBIAN_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "u44e <u44e@users.noreply.github.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "video")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "CamStreamEmu — reproduce a captured camera's video stream on CardputerZero")
# GStreamer runtime (OSS, LGPL/GPL — see THIRD_PARTY_NOTICES.md)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libfreetype6, gstreamer1.0-tools, gstreamer1.0-plugins-base, gstreamer1.0-plugins-good, gstreamer1.0-plugins-bad, gstreamer1.0-libav, libgstrtspserver-1.0-0")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
set(CPACK_STRIP_FILES TRUE)
set(CPACK_PACKAGE_FILE_NAME "camstreamemu_${PROJECT_VERSION}_arm64")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dist")
include(CPack)
