cmake_minimum_required(VERSION 2.6.0)

project(sector-cpc)

set(SOURCES
  sector-cpc.c
  cpm.c
  cpcemu.c
  amsdos.c
)

set(TEST_SOURCES
  tests.c
  cpm.c
  cpcemu.c
  amsdos.c
)

if (UNIX)
  set(LIB_DEPENDENCIES m)
endif()

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} ${LIB_DEPENDENCIES})

set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 90)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_EXTENSIONS false)

add_executable(tests ${TEST_SOURCES})
target_link_libraries(tests ${LIB_DEPENDENCIES})
