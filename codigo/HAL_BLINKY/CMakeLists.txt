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
project(HAL_BLINKY)

# Enable CMake support for ASM and C/CPP languages
enable_language(C ASM)

set(PROJECT_NAME HAL_BLINKY)
set(CMSIS_ROOT "/home/carolina/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.0")
# Create an executable object type
add_executable(${PROJECT_NAME})


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
