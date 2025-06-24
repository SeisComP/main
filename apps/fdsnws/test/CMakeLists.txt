ADD_DEFINITIONS("-DTEST_BUILD_DIR=\"${CMAKE_CURRENT_BINARY_DIR}\"")

ADD_TEST(
	NAME test_fdsnws_static
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test-static.py
)

ADD_TEST(
	NAME test_fdsnws_event
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test-event.py
)

ADD_TEST(
	NAME test_fdsnws_station
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test-station.py
)

ADD_TEST(
	NAME test_fdsnws_station_conditional
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test-station-conditional.py
)

ADD_TEST(
	NAME test_fdsnws_station_precision
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test-station-precision.py
)

ADD_TEST(
	NAME test_fdsnws_dataselect
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test-dataselect.py
)

ADD_TEST(
	NAME test_fdsnws_availability
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test-availability.py
)

SET(DATADIR "${CMAKE_CURRENT_SOURCE_DIR}/data")

# Set the system and user configuration directories with respect to the
# data directory to avoid reading installed and maybe tuned configurations.
SET_TESTS_PROPERTIES(
	test_fdsnws_static
	test_fdsnws_event
	test_fdsnws_station
	test_fdsnws_station_conditional
	test_fdsnws_station_precision
	test_fdsnws_dataselect
	test_fdsnws_availability
	PROPERTIES ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1;LD_LIBRARY_PATH=${PROJECT_BINARY_DIR}/lib;PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}:${PROJECT_BINARY_DIR}/lib/python;SEISCOMP_ROOT=${DATADIR};SEISCOMP_LOCAL_CONFIG=${CMAKE_CURRENT_BINARY_DIR}/.seiscomp")

