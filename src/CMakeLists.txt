cmake_minimum_required(VERSION 2.8)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../cmake")
set(NVM_PBLK_VERSION_MAJOR 0)
set(NVM_PBLK_VERSION_MINOR 0)
set(NVM_PBLK_VERSION_PATCH 1)
set(NVM_PBLK_VERSION "${NVM_PBLK_VERSION_MAJOR}.${NVM_PBLK_VERSION_MINOR}.${NVM_PBLK_VERSION_PATCH}")

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DNVM_DEBUG_ENABLED")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

project(nvm_pblk C)
include(FeatureSummary)
if (NOT CMAKE_VERSION VERSION_LESS "2.8.12")
include(FindOpenMP)
endif()
include(use_c99)
use_c99()

if (NOT CMAKE_VERSION VERSION_LESS "2.8.12")
include(FindOpenMP)
endif()
if (OPENMP_FOUND)
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
endif()

find_package(liblightnvm REQUIRED nvm_pblk)
find_package(udev REQUIRED nvm_pblk)

message("liblightnvm_cli(${liblightnvm_cli_LIBRARY})")

set(SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/nvm_pblk.c
)

#
# We link against the lightnvm_cli.a to avoid the runtime dependency
#
# NOTE: The dependency on libudev is carried on from liblightnvm
#
foreach(SRC_FN ${SOURCE_FILES})
	get_filename_component(SRC_FN_WE ${SRC_FN} NAME_WE)
	set(EXE_FN "${SRC_FN_WE}")
	add_executable(${EXE_FN} ${SRC_FN})
	target_include_directories(${EXE_FN} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	target_link_libraries(${EXE_FN} ${liblightnvm_cli_LIBRARY})
	target_link_libraries(${EXE_FN} ${liblightnvm_LIBRARY})
	target_link_libraries(${EXE_FN} udev ${CMAKE_THREAD_LIBS_INIT})

	install(TARGETS ${EXE_FN} DESTINATION bin)
endforeach()

# Packages
set(CPACK_GENERATOR "DEB" "TGZ")
set(CPACK_PACKAGE_VERSION "${NVM_PBLK_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${NVM_PBLK_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${NVM_PBLK_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${NVM_PBLK_VERSION_PATCH}")
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION}")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "nvm_pblk: CLI tool for pblk FTL")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "lightnvm.io")
if ("${CMAKE_VERSION}" VERSION_GREATER "2.8.7")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
endif()

include(CPack)
