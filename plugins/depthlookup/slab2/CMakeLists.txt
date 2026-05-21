SET(PLUGIN_TARGET dlslab2)

SET(
	PLUGIN_SOURCES
		plugin.cpp
		slab2depthlookup.cpp
)

SC_ADD_PLUGIN_LIBRARY(PLUGIN ${PLUGIN_TARGET} "")
SC_LINK_LIBRARIES_INTERNAL(${PLUGIN_TARGET} core)

TARGET_INCLUDE_DIRECTORIES(${PLUGIN_TARGET} PRIVATE .)

FILE(GLOB descs "${CMAKE_CURRENT_SOURCE_DIR}/descriptions/*.xml")
INSTALL(FILES ${descs} DESTINATION ${SC3_PACKAGE_APP_DESC_DIR})

# Install Slab2.0 BNA depth-footprint files (20 km intervals, 30 zones)
# to the plugin data dir (runtime use by dlslab2) and to the global spatial
# vector dir (GUI display layer "slab2" in scmv/scrttv/scesv)
INSTALL(
	DIRECTORY share/slabs/
	DESTINATION ${SC3_PACKAGE_SHARE_DIR}/dlslab2/slabs
)
INSTALL(
	DIRECTORY share/slabs/
	DESTINATION ${SC3_PACKAGE_SHARE_DIR}/spatial/vector/slab2
)
