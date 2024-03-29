ADD_DEFINITIONS("-DTEST_BUILD_DIR=\"${CMAKE_CURRENT_BINARY_DIR}\"")

ADD_TEST(
	NAME static
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test-static.py
)

ADD_TEST(
	NAME event
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test-event.py
)

ADD_TEST(
	NAME station
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test-station.py
)

ADD_TEST(
	NAME station-conditional
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test-station-conditional.py
)

ADD_TEST(
	NAME station-precision
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test-station-precision.py
)

ADD_TEST(
	NAME dataselect
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test-dataselect.py
)

ADD_TEST(
	NAME availability
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test-availability.py
)

SET(DATADIR "${CMAKE_CURRENT_SOURCE_DIR}/data")

# Set the system and user configuration directories with respect to the
# data directory to avoid reading installed and maybe tuned configurations.
SET_TESTS_PROPERTIES(static event station station-conditional station-precision dataselect availability
	PROPERTIES ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1;LD_LIBRARY_PATH=${PROJECT_BINARY_DIR}/lib;PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}:${PROJECT_BINARY_DIR}/lib/python;SEISCOMP_ROOT=${DATADIR};SEISCOMP_LOCAL_CONFIG=${CMAKE_CURRENT_BINARY_DIR}/.seiscomp")

