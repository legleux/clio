include(GNUInstallDirs)
set (CLIO_INSTALL_DIR "/opt/clio")
set (CMAKE_INSTALL_PREFIX ${CLIO_INSTALL_DIR})

install (TARGETS clio_server DESTINATION bin)

file (READ example-config.json config)
string (REGEX REPLACE "./clio_log" "/var/log/clio/" config "${config}")
file (WRITE ${CMAKE_BINARY_DIR}/install-config.json "${config}")
install (FILES ${CMAKE_BINARY_DIR}/install-config.json DESTINATION etc RENAME config.json)

configure_file ("${CMAKE_SOURCE_DIR}/CMake/install/clio.service.in" "${CMAKE_BINARY_DIR}/clio.service")

# only if systemd?
install (FILES "${CMAKE_BINARY_DIR}/clio.service" DESTINATION /lib/systemd/system)

# only if install_prefix is usr/local?
file(CREATE_LINK ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/clio_server /usr/local/bin/clio_server SYMBOLIC)
