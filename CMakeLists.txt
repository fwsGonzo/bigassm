cmake_minimum_required(VERSION 3.9)
project(fab128 CXX)

option(GPROF       "Enable profiling with gprof" OFF)
option(LINKER_GC   "Enable linker section GC" OFF)
option(LTO         "Enable interprocedural optimizations" ON)
option(NATIVE      "Enable native instructions" OFF)
option(SANITIZE    "Enable sanitizers" OFF)
option(STATIC_BUILD "Build a static executable" OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

set(SOURCES
	src/assemble.cpp
	src/directive.cpp
	src/elf64.cpp
	src/elf128.cpp
	src/hex128.cpp
	src/main.cpp
	src/opcodes.cpp
	src/pseudo_ops.cpp
	src/raw_split.cpp
	src/registers.cpp
	src/section.cpp
	src/token.cpp
	src/tokenizer.cpp
)

if (LTO)
	if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
	endif()
endif()
if (LINKER_GC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-gc-sections -Wl,-s")
endif()

function (add_assembler NAME)
	add_executable(${NAME} ${SOURCES})
	set_target_properties(${NAME} PROPERTIES CXX_STANDARD 17)
	target_compile_definitions(${NAME} PRIVATE ${ARGN})

	if (LTO)
		set_property(TARGET ${NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
	endif()
	if (SANITIZE)
		target_link_libraries(${NAME} "-fsanitize=address,undefined")
	endif()
	if (GPROF)
		target_link_libraries(${NAME} "-pg")
	endif()
	if (STATIC_BUILD)
		target_link_libraries(${NAME} "-static -static-libgcc")
	endif()
endfunction()

add_assembler(fab128)
