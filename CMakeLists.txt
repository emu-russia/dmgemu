cmake_minimum_required (VERSION 3.0)
project (dmgemu CXX)

add_definitions (-D_LINUX)

# Main application

set(CMAKE_BUILD_TYPE Release)

set(SDL_SHARED OFF)
set(SDL_STATIC ON)

find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)
find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2main)

add_executable (dmgemu 
	apu.cpp 
	gb.cpp
	introm.cpp
	lcd.cpp
	main.cpp
	mem.cpp
	misc.cpp
	pad.cpp
	perftimer.cpp
	ppu.cpp
	sm83.cpp
	sound.cpp
)

target_link_libraries (dmgemu LINK_PUBLIC SDL2)
