SET(PACKAGE_NAME TRACEVIEW)
SET(APP_NAME scrttv)

SET(
	${PACKAGE_NAME}_SOURCES
		main.cpp
		associator.cpp
		mainwindow.cpp
		spectrogramsettings.cpp
		statuslabel.cpp
		tracemarker.cpp
)

SET(
	${PACKAGE_NAME}_MOC_HEADERS
		associator.h
		mainwindow.h
		progressbar.h
		spectrogramsettings.h
		statuslabel.h
)

SET(${PACKAGE_NAME}_UI associator.ui mainwindow.ui spectrogramsettings.ui)


SC_ADD_GUI_EXECUTABLE(${PACKAGE_NAME} ${APP_NAME})
SC_LINK_LIBRARIES_INTERNAL(${APP_NAME} qt)

SC_INSTALL_INIT(${APP_NAME} ${INIT_ND_NB_TEMPLATE})

FILE(GLOB descs "${CMAKE_CURRENT_SOURCE_DIR}/descriptions/*.xml")
INSTALL(FILES ${descs} DESTINATION ${SC3_PACKAGE_APP_DESC_DIR})
