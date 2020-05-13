################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
LD_SRCS += \
../src/lscript.ld 

C_SRCS += \
../src/main.c \
../src/platform.c \
../src/ps7_init.c \
../src/user_dma.c \
../src/user_gpio.c \
../src/user_interrupts.c \
../src/user_tests.c 

OBJS += \
./src/main.o \
./src/platform.o \
./src/ps7_init.o \
./src/user_dma.o \
./src/user_gpio.o \
./src/user_interrupts.o \
./src/user_tests.o 

C_DEPS += \
./src/main.d \
./src/platform.d \
./src/ps7_init.d \
./src/user_dma.d \
./src/user_gpio.d \
./src/user_interrupts.d \
./src/user_tests.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: ARM v7 gcc compiler'
	arm-none-eabi-gcc -Wall -O2 -c -fmessage-length=0 -MT"$@" -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=hard -I/home/gerard/Documents/TFG/ZynqDmaAnalysis/SW/env1_cora/env1_cora/export/env1_cora/sw/env1_cora/standalone_domain/bspinclude/include -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


