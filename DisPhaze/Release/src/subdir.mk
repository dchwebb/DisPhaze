################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (9-2020-q2-update)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_SRCS += \
../src/startup_stm32f40xx.s 

C_SRCS += \
../src/stm32f4xx_flash.c \
../src/system_stm32f4xx.c 

CPP_SRCS += \
../src/LUT.cpp \
../src/PhaseDistortion.cpp \
../src/config.cpp \
../src/initialisation.cpp \
../src/main.cpp 

S_DEPS += \
./src/startup_stm32f40xx.d 

C_DEPS += \
./src/stm32f4xx_flash.d \
./src/system_stm32f4xx.d 

OBJS += \
./src/LUT.o \
./src/PhaseDistortion.o \
./src/config.o \
./src/initialisation.o \
./src/main.o \
./src/startup_stm32f40xx.o \
./src/stm32f4xx_flash.o \
./src/system_stm32f4xx.o 

CPP_DEPS += \
./src/LUT.d \
./src/PhaseDistortion.d \
./src/config.d \
./src/initialisation.d \
./src/main.d 


# Each subdirectory must supply rules for building sources it contributes
src/LUT.o: ../src/LUT.cpp src/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=gnu++11 -DSTM32F4XX -DSTM32F40XX -DSTM32F405XX -DSTM32F405xx -c -I../src -I../Libraries/CMSIS/Include -I../Libraries/Device/STM32F4xx/Include -I../Libraries/STM32F4xx_StdPeriph_Driver/inc -Ofast -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -MMD -MP -MF"src/LUT.d" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
src/PhaseDistortion.o: ../src/PhaseDistortion.cpp src/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=gnu++11 -DSTM32F4XX -DSTM32F40XX -DSTM32F405XX -DSTM32F405xx -c -I../src -I../Libraries/CMSIS/Include -I../Libraries/Device/STM32F4xx/Include -I../Libraries/STM32F4xx_StdPeriph_Driver/inc -Ofast -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -MMD -MP -MF"src/PhaseDistortion.d" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
src/config.o: ../src/config.cpp src/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=gnu++11 -DSTM32F4XX -DSTM32F40XX -DSTM32F405XX -DSTM32F405xx -c -I../src -I../Libraries/CMSIS/Include -I../Libraries/Device/STM32F4xx/Include -I../Libraries/STM32F4xx_StdPeriph_Driver/inc -Ofast -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -MMD -MP -MF"src/config.d" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
src/initialisation.o: ../src/initialisation.cpp src/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=gnu++11 -DSTM32F4XX -DSTM32F40XX -DSTM32F405XX -DSTM32F405xx -c -I../src -I../Libraries/CMSIS/Include -I../Libraries/Device/STM32F4xx/Include -I../Libraries/STM32F4xx_StdPeriph_Driver/inc -Ofast -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -MMD -MP -MF"src/initialisation.d" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
src/main.o: ../src/main.cpp src/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=gnu++11 -DSTM32F4XX -DSTM32F40XX -DSTM32F405XX -DSTM32F405xx -c -I../src -I../Libraries/CMSIS/Include -I../Libraries/Device/STM32F4xx/Include -I../Libraries/STM32F4xx_StdPeriph_Driver/inc -Ofast -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -MMD -MP -MF"src/main.d" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
src/startup_stm32f40xx.o: ../src/startup_stm32f40xx.s src/subdir.mk
	arm-none-eabi-gcc -mcpu=cortex-m4 -DSTM32F4XX -DSTM32F40XX -DSTM32F405XX -DSTM32F405xx -c -I../src -I../Libraries/CMSIS/Include -I../Libraries/Device/STM32F4xx/Include -I../Libraries/STM32F4xx_StdPeriph_Driver/inc -x assembler-with-cpp -MMD -MP -MF"src/startup_stm32f40xx.d" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@" "$<"
src/stm32f4xx_flash.o: ../src/stm32f4xx_flash.c src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -DSTM32F4XX -DSTM32F40XX -DSTM32F405XX -DSTM32F405xx -c -I../src -I../Libraries/CMSIS/Include -I../Libraries/Device/STM32F4xx/Include -I../Libraries/STM32F4xx_StdPeriph_Driver/inc -Ofast -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"src/stm32f4xx_flash.d" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
src/system_stm32f4xx.o: ../src/system_stm32f4xx.c src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -DSTM32F4XX -DSTM32F40XX -DSTM32F405XX -DSTM32F405xx -c -I../src -I../Libraries/CMSIS/Include -I../Libraries/Device/STM32F4xx/Include -I../Libraries/STM32F4xx_StdPeriph_Driver/inc -Ofast -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"src/system_stm32f4xx.d" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

