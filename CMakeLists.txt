cmake_minimum_required(VERSION 2.6)
project(sensor CXX)

SET(PREFIX ${CMAKE_INSTALL_PREFIX})
SET(EXEC_PREFIX "\${prefix}")
SET(LIBDIR "\${prefix}/lib")
SET(INCLUDEDIR "\${prefix}/include")
SET(VERSION_MAJOR 1)
SET(VERSION "${VERSION_MAJOR}.1.0")

include(FindPkgConfig)
pkg_check_modules(rpkgs REQUIRED sf_common vconf glib-2.0)
add_definitions(${rpkgs_CFLAGS})

#add_definitions(-Wall -O3 -omit-frame-pointer -lm) 
#add_definitions(-Wall -g -lma -DUSE_FILE_DEBUG)
add_definitions(-Wall -g -lma -D_DEBUG)
#add_definitions(-fvisibility=hidden -lm -DUSE_DLOG_LOG)
#add_definitions(-fvisibility=hidden -lm)

FIND_PROGRAM(UNAME NAMES uname)
EXEC_PROGRAM("${UNAME}" ARGS "-m" OUTPUT_VARIABLE "ARCH")
IF("${ARCH}" STREQUAL "arm")
#        ADD_DEFINITIONS("-DTARGET -DHWREV_CHECK -DUSE_MPU3050_GYRO")
	ADD_DEFINITIONS("-DTARGET -DHWREV_CHECK")
        MESSAGE("add -DTARGET")
ELSE("${ARCH}" STREQUAL "arm")
        ADD_DEFINITIONS("-DSIMULATOR")
        MESSAGE("add -DSIMULATOR")
ENDIF("${ARCH}" STREQUAL "arm")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(${PROJECT_NAME} SHARED 
	src/client.cpp
)

#add_dependencies(${PROJECT_NAME} sf_common)
# to install pkgconfig setup file.

target_link_libraries(${PROJECT_NAME} ${rpkgs_LDFLAGS} ${GLES_LDFLAGS})
SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES SOVERSION ${VERSION_MAJOR})
SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES VERSION ${VERSION})

configure_file(${PROJECT_NAME}.pc.in ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.pc @ONLY)

#install(DIRECTORY include/ DESTINATION include/ FILES_MATCHING PATTERN "*.h")
install(TARGETS ${PROJECT_NAME} DESTINATION lib COMPONENT RuntimeLibraries)

install(FILES include/sensor.h DESTINATION include/sensor/)
install(FILES include/sensor_accel.h DESTINATION include/sensor/)
install(FILES include/sensor_geomag.h DESTINATION include/sensor/)
install(FILES include/sensor_proxi.h DESTINATION include/sensor/)
install(FILES include/sensor_motion.h DESTINATION include/sensor/)
install(FILES include/sensor_light.h DESTINATION include/sensor/)
install(FILES include/sensor_gyro.h DESTINATION include/sensor/)
install(FILES include/sensor_barometer.h DESTINATION include/sensor/)
install(FILES include/sensor_fusion.h DESTINATION include/sensor/)

install(FILES ${PROJECT_NAME}.pc DESTINATION lib/pkgconfig)
