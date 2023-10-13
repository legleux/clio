set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "http://github.com/XRPLF/clio")
set(CPACK_SYSTEM_NAME "amd64")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${CPACK_SYSTEM_NAME}")

set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "rippled")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Ripple Labs Inc. <support@ripple.com>")

set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
  ${CMAKE_SOURCE_DIR}/CMake/packaging/deb/conffiles
  ${CMAKE_SOURCE_DIR}/CMake/packaging/deb/postinst
)

set(CPACK_DEBIAN_PACKAGE_RELEASE 1)
set(CPACK_PACKAGE_VERSION ${VERSION})
message("CPACK_PACKAGE_VERSION: ${CPACK_PACKAGE_VERSION}")
string(REPLACE "-" ";" VERSION_LIST ${CPACK_PACKAGE_VERSION})
message("VERSION_LIST: ${VERSION_LIST}")
list(GET VERSION_LIST 0 CPACK_PACKAGE_VERSION)
list (LENGTH VERSION_LIST _len)
if (${_len} GREATER 1)
    list(GET VERSION_LIST 1 PRE_VERSION)
    message("PRE_VERSION: ${PRE_VERSION}")
    set(CPACK_DEBIAN_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION}~${PRE_VERSION}")
else()
    set(CPACK_DEBIAN_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION}")

endif()
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}_${CPACK_DEBIAN_PACKAGE_VERSION}-${CPACK_DEBIAN_PACKAGE_RELEASE}_${CPACK_SYSTEM_NAME}")
