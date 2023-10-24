SET(APPRELDIR "..")
SET(testSrc scardac.cpp ${APPRELDIR}/scardac.cpp)
SET(testName test_scardac)

INCLUDE_DIRECTORIES(${APPRELDIR}/libs)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR}/${APPRELDIR}/libs)

ADD_EXECUTABLE(${testName} ${testSrc})
SC_LINK_LIBRARIES_INTERNAL(${testName} unittest core client daplugin)
SC_LINK_LIBRARIES(${testName})

ADD_TEST(
	NAME ${testName}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND ${testName}
)

SET(DATADIR "${CMAKE_CURRENT_SOURCE_DIR}/data")

# Set the system and user configuration directories with respect to the
# data directory to avoid reading installed and maybe tuned configurations.
SET_TESTS_PROPERTIES(${testName}
	PROPERTIES ENVIRONMENT "LD_LIBRARY_PATH=${PROJECT_BINARY_DIR}/lib;SEISCOMP_ROOT=${DATADIR};SEISCOMP_LOCAL_CONFIG=${CMAKE_CURRENT_BINARY_DIR}/.seiscomp")
