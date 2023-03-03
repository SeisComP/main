SET(PACKAGE_NAME TRACEVIEW)
SET(APP_NAME scrttv)

SET(
	${PACKAGE_NAME}_SOURCES
		main.cpp
		associator.cpp
		mainwindow.cpp
		statuslabel.cpp
		tracemarker.cpp
)

SET(
	${PACKAGE_NAME}_MOC_HEADERS
		associator.h
		mainwindow.h
		progressbar.h
		statuslabel.h
)

SET(${PACKAGE_NAME}_UI associator.ui mainwindow.ui)


SC_ADD_GUI_EXECUTABLE(${PACKAGE_NAME} ${APP_NAME})
SC_LINK_LIBRARIES_INTERNAL(${APP_NAME} qt)

FILE(GLOB descs "${CMAKE_CURRENT_SOURCE_DIR}/descriptions/*.xml")
INSTALL(FILES ${descs} DESTINATION ${SC3_PACKAGE_APP_DESC_DIR})
