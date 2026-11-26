# Set the minimum version of CMake that can be used
cmake_minimum_required(VERSION 3.20)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)

# Set target definition variables required by the toolchain file
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/target.cmake)

# Set the toolchain file for cross-compilation
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/gcc-arm-none-eabi.cmake)

# Project definition
project(tarea_3)

# Enable CMake support for ASM and C/CPP languages
enable_language(C ASM)

set(PROJECT_NAME tarea_3)
set(CMSIS_ROOT "/home/carolina/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.0")
# Create an executable object type
add_executable(${PROJECT_NAME})

# ---- SELECTOR DE APLICACIÓN ---------------------------------------------
# One fixed main.c, several applications: APP decides which app.c is compiled.
# Each new stage adds its name to the STRINGS list and one APP_DIR_<name> line.
set(APP "blinky" CACHE STRING "Application to build")
set_property(CACHE APP PROPERTY STRINGS blinky)

set(APP_DIR_blinky ${CMAKE_SOURCE_DIR}/Core/app_0_blinky)

if(NOT DEFINED APP_DIR_${APP})
    message(FATAL_ERROR "Unknown APP='${APP}': there is no APP_DIR_${APP} line in CMakeLists.txt")
endif()

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/Core/common)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${APP_DIR_${APP}}/app.c)
message(STATUS ">>> Building APP = ${APP}")
# --------------------------------------------------------------------------

#Agrego archivos que he añadido para stage 0:
# Estos son archivos comunes para las diferentes aplicaciones
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
${CMAKE_SOURCE_DIR}/Core/common/system.c
${CMAKE_SOURCE_DIR}/Core/common/heartbeat.c
)

# Include additional CMake files for flags, components, and files
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/flags.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/components.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/files.cmake)

# Add sources to executable/library
# PASO 9 - HAL BLINKY TUTORIAL:
target_sources(${PROJECT_NAME} PRIVATE
  ${CMAKE_SOURCE_DIR}/Core/Src/system_stm32f4xx.c

  ${CMAKE_SOURCE_DIR}/Core/Src/stm32f4xx_it.c

  # HAL drivers — one line per file copied in Step 4
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
  ${CMAKE_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
)

# Add include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/Inc                   # stm32f4xx_hal_conf.h
    ${CMSIS_ROOT}/Drivers/STM32F4xx_HAL_Driver/Inc # HAL headers
    ${CMSIS_ROOT}/Drivers/CMSIS/Core/Include      # core_cm4.h
    ${CMSIS_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Include # stm32f4xx.h
    # User defined include directories
)

# Compiler options
target_compile_options(${PROJECT_NAME} PRIVATE
  # User defined compiler options
)

# Add additional defined symbols
target_compile_definitions(${PROJECT_NAME} PRIVATE
  STM32F411xE
  # Add additional defined symbols here
)

# Linker options
target_link_options(${PROJECT_NAME} PRIVATE 
  # User defined linker options
)

# Add additional linker search directories
target_link_directories(${PROJECT_NAME} PRIVATE
  # Add additional linker search directories here
)

# Add additional linker libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
  # Add additional linker libraries here
)
