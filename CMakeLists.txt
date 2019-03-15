cmake_minimum_required(VERSION 2.6.0)

project(sector-cpc)

set(SOURCES
  sector-cpc.c
  cpm.c
  cpcemu.c
)

add_executable(${PROJECT_NAME} ${SOURCES})

set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 90)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_EXTENSIONS false)

