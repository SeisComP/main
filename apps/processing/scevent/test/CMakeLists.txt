ADD_DEFINITIONS("-DTEST_BUILD_DIR=\"${CMAKE_CURRENT_BINARY_DIR}\"")

ADD_TEST(
	NAME scevent-offline
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test-offline.py
)

ADD_TEST(
	NAME scevent-remote
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test-remote.py
)

#ADD_TEST(
#	NAME test-scevent-api
#	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test-event.py
#)

SET(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")

# Set the system and user configuration directories with respect to the
# data directory to avoid reading installed and maybe tuned configurations.
SET_TESTS_PROPERTIES(
	scevent-offline
	scevent-remote
	PROPERTIES ENVIRONMENT "\
PATH=${PROJECT_BINARY_DIR}/bin;\
PYTHONDONTWRITEBYTECODE=1;\
LD_LIBRARY_PATH=${PROJECT_BINARY_DIR}/lib;\
PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}:${PROJECT_BINARY_DIR}/lib/python;\
SEISCOMP_ROOT=${ROOT_DIR};\
SEISCOMP_LOCAL_CONFIG=${CMAKE_CURRENT_BINARY_DIR}/.seiscomp"
)
