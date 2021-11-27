INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_PATH})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

SET(wrapper_module autoloc)

SET(WRAPPER_MODULE_autoloc_EXTRA_SC_LIBS core autoloc)

SET(
	${wrapper_module}_SOURCES
		${wrapper_module}_python_wrap.cxx
		${WRAPPER_MODULE_${wrapper_module}_EXTRA_FILES}
	)
ADD_LIBRARY(_${wrapper_module} MODULE ${${wrapper_module}_SOURCES})
SET_TARGET_PROPERTIES(_${wrapper_module} PROPERTIES PREFIX "")
TARGET_LINK_LIBRARIES(_${wrapper_module} ${PYTHON_LIBRARIES})
TARGET_LINK_LIBRARIES(_${wrapper_module} ${WRAPPER_MODULE_${wrapper_module}_EXTRA_LIBS})
SC_LINK_LIBRARIES_INTERNAL(_${wrapper_module} ${WRAPPER_MODULE_${wrapper_module}_EXTRA_SC_LIBS})

SC_SWIG_GET_MODULE_PATH(_module_path)
SET(_module_path ${SC3_PACKAGE_PYTHON_LIB_DIR}/seiscomp)

INSTALL(
	TARGETS _${wrapper_module}
	LIBRARY DESTINATION ${_module_path}
)

INSTALL(
	FILES ${CMAKE_CURRENT_SOURCE_DIR}/${wrapper_module}.py
	DESTINATION ${_module_path}
)

INSTALL(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/${wrapper_module}.pyc
	DESTINATION ${_module_path}
	OPTIONAL
)

INSTALL(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/${wrapper_module}.pyo
	DESTINATION ${_module_path}
	OPTIONAL
)

ADD_CUSTOM_COMMAND(
	TARGET _${wrapper_module}
	POST_BUILD
	COMMAND ${CMAKE_SOURCE_DIR}/py-compile ARGS --destdir ${CMAKE_CURRENT_BINARY_DIR} ${wrapper_module}.py
)
