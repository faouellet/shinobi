cmake_minimum_required(VERSION 3.13)
project(shinobi)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost COMPONENTS system REQUIRED)

find_package(Threads)

if(CMAKE_BUILD_TYPE MATCHES "Release")
	cmake_policy(SET CMP0069 NEW)
	include(CheckIPOSupported)
	check_ipo_supported(RESULT lto_supported OUTPUT error)

	if(lto_supported)
		message(STATUS "IPO / LTO enabled")
		set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
	else()
		message(STATUS "IPO / LTO not supported: <${error}>")
	endif()
endif()

if(MSVC)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /GR- /Zc:__cplusplus")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated -fdiagnostics-color")
endif()

find_program(RE2C re2c)
if(RE2C)
	# the depfile parser and shinobi lexers are generated using re2c.
	function(re2c IN OUT)
		add_custom_command(DEPENDS ${IN} OUTPUT ${OUT}
			COMMAND ${RE2C} -b -i --no-generation-date -o ${OUT} ${IN}
		)
	endfunction()
	re2c(${CMAKE_SOURCE_DIR}/src/depfile_parser.in.cc ${CMAKE_BINARY_DIR}/depfile_parser.cc)
	re2c(${CMAKE_SOURCE_DIR}/src/lexer.in.cc ${CMAKE_BINARY_DIR}/lexer.cc)
	add_library(libshinobi-re2c OBJECT ${CMAKE_BINARY_DIR}/depfile_parser.cc ${CMAKE_BINARY_DIR}/lexer.cc)
else()
	message(WARNING "re2c was not found; changes to src/*.in.cc will not affect your build.")
	add_library(libshinobi-re2c OBJECT src/depfile_parser.cc src/lexer.cc)
endif()
target_include_directories(libshinobi-re2c PRIVATE src)

# Core source files all build into the daemon library
add_library(libdaemon OBJECT
	src/daemon.cc
	src/dcache.cc
)

# Core source files all build into shinobi library.
add_library(libshinobi OBJECT
	src/build_log.cc
	src/build.cc
	src/clean.cc
	src/clparser.cc
	src/dyndep.cc
	src/dyndep_parser.cc
	src/debug_flags.cc
	src/deps_log.cc
	src/disk_interface.cc
	src/edit_distance.cc
	src/eval_env.cc
	src/graph.cc
	src/graphviz.cc
	src/host_parser.cc
	src/line_printer.cc
	src/manifest_parser.cc
	src/metrics.cc
	src/parser.cc
	src/state.cc
	src/string_view_util.cc
	src/util.cc
	src/version.cc
)
if(WIN32)
	target_sources(libshinobi PRIVATE
		src/subprocess-win32.cc
		src/includes_normalize-win32.cc
		src/msvc_helper-win32.cc
		src/msvc_helper_main-win32.cc
		src/getopt.c
	)
	if(MSVC)
		target_sources(libshinobi PRIVATE src/minidump-win32.cc)
	endif()
else()
	target_sources(libshinobi PRIVATE src/subprocess-posix.cc)
endif()

#Fixes GetActiveProcessorCount on MinGW
if(MINGW)
target_compile_definitions(libshinobi PRIVATE _WIN32_WINNT=0x0601 __USE_MINGW_ANSI_STDIO=1)
endif()

# Distributed build support
add_executable(daemon_exec 
	src/daemon_exec.cc
)
target_include_directories(daemon_exec PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(daemon_exec PRIVATE libdaemon Boost::system ${CMAKE_THREAD_LIBS_INIT})

# Main executable is library plus main() function.
add_executable(shinobi src/ninja.cc)
target_include_directories(shinobi PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(shinobi PRIVATE libshinobi libshinobi-re2c libdaemon Boost::system ${CMAKE_THREAD_LIBS_INIT})

# Tests all build into shinobi_test executable.
add_executable(shinobi_test
	src/build_log_test.cc
	src/build_test.cc
	src/clean_test.cc
	src/clparser_test.cc
    src/dcache_test.cc
	src/depfile_parser_test.cc
	src/deps_log_test.cc
	src/disk_interface_test.cc
	src/dyndep_parser_test.cc
	src/edit_distance_test.cc
	src/graph_test.cc
    src/host_parser_test.cc
	src/lexer_test.cc
	src/manifest_parser_test.cc
	src/ninja_test.cc
	src/state_test.cc
	src/string_view_util_test.cc
	src/subprocess_test.cc
	src/test.cc
	src/util_test.cc
)
if(WIN32)
	target_sources(shinobi_test PRIVATE src/includes_normalize_test.cc src/msvc_helper_test.cc)
endif()
target_include_directories(shinobi_test PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(shinobi_test PRIVATE libshinobi libshinobi-re2c libdaemon Boost::system ${CMAKE_THREAD_LIBS_INIT})

foreach(perftest
  build_log_perftest
  canon_perftest
  clparser_perftest
  depfile_parser_perftest
  hash_collision_bench
  manifest_parser_perftest
)
  add_executable(${perftest} src/${perftest}.cc)
  target_include_directories(shinobi_test PRIVATE ${Boost_INCLUDE_DIRS})
  target_link_libraries(${perftest} PRIVATE libshinobi libshinobi-re2c libdaemon Boost::system ${CMAKE_THREAD_LIBS_INIT})
endforeach()

enable_testing()
add_test(NinjaTest shinobi_test)
