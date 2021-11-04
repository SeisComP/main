SUBDIRS(collector)

SET(DAPLUGIN_SOURCES
	collector.cpp
	collector/sds.cpp
)

SET(DAPLUGIN_HEADERS
	collector.h
)

SC_ADD_LIBRARY(DAPLUGIN daplugin)

# Install the headers in install/seiscomp/plugins/dataavailabilty
SC_LIB_INSTALL_HEADERS(DAPLUGIN seiscomp/plugins/dataavailabilty)
SC_LIB_LINK_LIBRARIES_INTERNAL(daplugin core)
