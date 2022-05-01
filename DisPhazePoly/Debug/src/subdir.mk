################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (10.3-2021.10)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_SRCS += \
../src/startup_stm32f40xx.s 

C_SRCS += \
../src/system_stm32f4xx.c 

CPP_SRCS += \
../src/CDCHandler.cpp \
../src/LUT.cpp \
../src/PhaseDistortion.cpp \
../src/USB.cpp \
../src/config.cpp \
../src/initialisation.cpp \
../src/main.cpp 

S_DEPS += \
./src/startup_stm32f40xx.d 

C_DEPS += \
./src/system_stm32f4xx.d 

OBJS += \
./src/CDCHandler.o \
./src/LUT.o \
./src/PhaseDistortion.o \
./src/USB.o \
./src/config.o \
./src/initialisation.o \
./src/main.o \
./src/startup_stm32f40xx.o \
./src/system_stm32f4xx.o 

CPP_DEPS += \
./src/CDCHandler.d \
./src/LUT.d \
./src/PhaseDistortion.d \
./src/USB.d \
./src/config.d \
./src/initialisation.d \
./src/main.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o src/%.su: ../src/%.cpp src/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=gnu++17 -g3 -DSTM32F405xx -DDEBUG -c -I../src -I../Libraries/CMSIS/Include -I../Libraries/Device/STM32F4xx/Include -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
src/%.o: ../src/%.s src/subdir.mk
	arm-none-eabi-gcc -mcpu=cortex-m4 -g3 -DSTM32F405xx -DDEBUG -c -I../src -I../Libraries/CMSIS/Include -I../Libraries/Device/STM32F4xx/Include -x assembler-with-cpp -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@" "$<"
src/%.o src/%.su: ../src/%.c src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DSTM32F405xx -DDEBUG -c -I../src -I../Libraries/CMSIS/Include -I../Libraries/Device/STM32F4xx/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-src

clean-src:
	-$(RM) ./src/CDCHandler.d ./src/CDCHandler.o ./src/CDCHandler.su ./src/LUT.d ./src/LUT.o ./src/LUT.su ./src/PhaseDistortion.d ./src/PhaseDistortion.o ./src/PhaseDistortion.su ./src/USB.d ./src/USB.o ./src/USB.su ./src/config.d ./src/config.o ./src/config.su ./src/initialisation.d ./src/initialisation.o ./src/initialisation.su ./src/main.d ./src/main.o ./src/main.su ./src/startup_stm32f40xx.d ./src/startup_stm32f40xx.o ./src/system_stm32f4xx.d ./src/system_stm32f4xx.o ./src/system_stm32f4xx.su

.PHONY: clean-src

