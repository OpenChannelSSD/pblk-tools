# - Try to find liblightnvm
# Once done, this will define
#
#  liblightnvm_FOUND - system has liblightnvm
#  liblightnvm_INCLUDE_DIR - the liblightnvm include directories
#  liblightnvm_LIBRARY - link these to use liblightnvm
#  liblightnvm_cli_FOUND - system has liblightnvm_cli
#  liblightnvm_cli_INCLUDE_DIR - the liblightnvm_cli include directories
#  liblightnvm_cli_LIBRARY - link these to use liblightnvm_cli


include(LibFindMacros)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(liblightnvm_PKGCONF liblightnvm)

# Include dir
find_path(liblightnvm_INCLUDE_DIR
  NAMES liblightnvm.h
  PATHS ${liblightnvm_PKGCONF_INCLUDE_DIR}
)

# Finally the library itself
find_library(liblightnvm_LIBRARY
  NAMES liblightnvm.a
  PATHS ${liblightnvm_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(liblightnvm_PROCESS_INCLUDES liblightnvm_INCLUDE_DIR liblightnvm_INCLUDE_DIR)
set(liblightnvm_PROCESS_LIBS liblightnvm_LIBRARY liblightnvm_LIBRARY)
libfind_process(liblightnvm)

# Use pkg-config to get hints about paths
libfind_pkg_check_modules(liblightnvm_cli_PKGCONF liblightnvm_cli)

# Include dir
find_path(liblightnvm_cli_INCLUDE_DIR
  NAMES liblightnvm_cli.h
  PATHS ${liblightnvm_cli_PKGCONF_INCLUDE_DIR}
)

# Finally the library itself
find_library(liblightnvm_cli_LIBRARY
  NAMES liblightnvm_cli.a
  PATHS ${liblightnvm_cli_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(liblightnvm_cli_PROCESS_INCLUDES liblightnvm_cli_INCLUDE_DIR liblightnvm_cli_INCLUDE_DIR)
set(liblightnvm_cli_PROCESS_LIBS liblightnvm_cli_LIBRARY liblightnvm_cli_LIBRARY)
libfind_process(liblightnvm_cli)
