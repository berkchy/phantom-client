#
# Copyright (c) 2015 Pavlo Lavrenenko
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

cmake_minimum_required(VERSION 3.5)
project(MAINUI)

set(CMAKE_CXX_STANDARD 11)

option(MAINUI_FONT_SCALE "Scale fonts by height" OFF)

if(NOT XASH_SDK)
	set(XASH_SDK "sdk_includes/")
endif()

if(WIN32)
	add_definitions(-DMAINUI_USE_CUSTOM_FONT_RENDER)
endif()

file(GLOB MAINUI_CONTROLS_SOURCES "controls/*.cpp")
file(GLOB MAINUI_MENUS_SOURCES "menus/*.cpp" "menus/dynamic/*.cpp" "menus/client/*.cpp")
list(APPEND MAINUI_MENUS_SOURCES "menus/FontOptions.cpp")
list(APPEND MAINUI_MENUS_SOURCES "menus/HudFeedback.cpp")
file(GLOB MAINUI_FONT_SOURCES
	"font/BitmapFont.cpp"
	"font/BaseFontBackend.cpp"
	"font/FontManager.cpp"
	"font/FreeTypeFont.cpp"
	"font/StbFont.cpp"
	"font/WinAPIFont.cpp")
file(GLOB MAINUI_SOURCES "miniutl/*.cpp" "*.cpp")

add_library(menu SHARED ${MAINUI_CONTROLS_SOURCES} ${MAINUI_MENUS_SOURCES} ${MAINUI_FONT_SOURCES} ${MAINUI_SOURCES} ${CMAKE_SOURCE_DIR}/common/interface.cpp)

if(NOT WIN32 AND NOT MINGW)
	add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-variable)
endif()

add_definitions(-DSTDINT_H=<cstdint>)

include_directories(${XASH_SDK}/common ${XASH_SDK}/engine ${XASH_SDK}/pm_shared ${XASH_SDK}/public . controls/ menus/ miniutl/ font/ model/ ${CMAKE_SOURCE_DIR}/common ${CMAKE_SOURCE_DIR}/public)

if(MSVC)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

if(NOT BUILD_AS_PART_OF_ENGINE)
	# Engine expects "libmenu.so" (MENUDLL from port.h)
	set_target_properties(menu PROPERTIES OUTPUT_NAME "menu")
	# Output name must match MENUDLL from common/port.h
	if(NOT DEFINED GAMEDIR AND DEFINED GAME_DIR)
		# parent project provides GAME_DIR, use it as GAMEDIR for compatibility
		set(GAMEDIR "${GAME_DIR}")
	endif()
else()
	# install to the same directory as engine
        set(GAMEDIR ".")
        set(CLIENT_INSTALL_DIR ".")
endif()

install(TARGETS menu DESTINATION "${GAMEDIR}/${CLIENT_INSTALL_DIR}")

if(MSVC)
	install(FILES $<TARGET_PDB_FILE:menu> DESTINATION "${GAMEDIR}/${CLIENT_INSTALL_DIR}" OPTIONAL)
endif()
