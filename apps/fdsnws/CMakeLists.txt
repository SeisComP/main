#IF(CMAKE_MAJOR_VERSION GREATER 2)
SUBDIRS(test)
#ENDIF(CMAKE_MAJOR_VERSION GREATER 2)

SC_ADD_PYTHON_PROG(fdsnws)
SC_INSTALL_INIT(fdsnws initd.py)

# python library files
SET(PYTHON_PACKAGE_PATH ${SC3_PACKAGE_PYTHON_LIB_DIR}/seiscomp)
INSTALL(DIRECTORY fdsnws DESTINATION ${PYTHON_PACKAGE_PATH}
	PATTERN ".svn" EXCLUDE
	PATTERN "CMakeLists.txt" EXCLUDE
	PATTERN "*~" EXCLUDE
	PATTERN "*.pyc" EXCLUDE)

# add files to temporary lib folder in build directory for testing
SET(build_lib_python_seiscomp_dir ${CMAKE_BINARY_DIR}/lib/python/seiscomp/fdsnws)
FILE(MAKE_DIRECTORY ${build_lib_python_seiscomp_dir})
FILE(
	GLOB python_files
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/fdsnws
	"fdsnws/*.py"
)
FOREACH(file ${python_files})
	ADD_CUSTOM_TARGET(link_build_lib_python_seiscomp_fdsnws_${file} ALL
		COMMAND ${CMAKE_COMMAND} -E create_symlink
			${CMAKE_CURRENT_SOURCE_DIR}/fdsnws/${file}
			${build_lib_python_seiscomp_dir}/${file}
	)
ENDFOREACH()

# data files
SET(FDSNWS_SHARE_PATH ${SC3_PACKAGE_SHARE_DIR}/fdsnws)
INSTALL(DIRECTORY share/ DESTINATION ${FDSNWS_SHARE_PATH} USE_SOURCE_PERMISSIONS
	PATTERN ".svn" EXCLUDE
	PATTERN "*~" EXCLUDE)

# descriptions
FILE(GLOB descs "${CMAKE_CURRENT_SOURCE_DIR}/descriptions/*.xml")
INSTALL(FILES ${descs} DESTINATION ${SC3_PACKAGE_APP_DESC_DIR})
