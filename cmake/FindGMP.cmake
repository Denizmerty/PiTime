# GMP_ROOT may point to an installation prefix. The C API avoids a gmpxx dependency.
find_path(GMP_INCLUDE_DIR NAMES gmp.h HINTS ${GMP_ROOT} ENV GMP_ROOT PATH_SUFFIXES include)
find_library(GMP_LIBRARY NAMES gmp libgmp HINTS ${GMP_ROOT} ENV GMP_ROOT PATH_SUFFIXES lib)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GMP REQUIRED_VARS GMP_INCLUDE_DIR GMP_LIBRARY)
if(GMP_FOUND AND NOT TARGET GMP::GMP)
    add_library(GMP::GMP UNKNOWN IMPORTED)
    set_target_properties(GMP::GMP PROPERTIES IMPORTED_LOCATION "${GMP_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GMP_INCLUDE_DIR}")
endif()
mark_as_advanced(GMP_INCLUDE_DIR GMP_LIBRARY)
