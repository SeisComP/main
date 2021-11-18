SET(LIBAUTOLOC_SOURCES
	associator.cpp
	autoloc.cpp
	config.cpp
	datamodel.cpp
	locator.cpp
	nucleator.cpp
	sc3adapters.cpp
	stationconfig.cpp
	stationlocationfile.cpp
	util.cpp
)

SET(LIBAUTOLOC_HEADERS
	associator.h
	autoloc.h
	datamodel.h
	locator.h
	nucleator.h
	stationconfig.h
	stationlocationfile.h
#	util.h
)

SC_ADD_LIBRARY(LIBAUTOLOC autoloc)
SC_LIB_INSTALL_HEADERS(LIBAUTOLOC)
