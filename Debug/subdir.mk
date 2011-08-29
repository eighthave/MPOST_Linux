################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Acceptor.cpp \
../Bill.cpp \
../Coupon.cpp \
../DataLinkLayer.cpp \
../Worker.cpp 

OBJS += \
./Acceptor.o \
./Bill.o \
./Coupon.o \
./DataLinkLayer.o \
./Worker.o 

CPP_DEPS += \
./Acceptor.d \
./Bill.d \
./Coupon.d \
./DataLinkLayer.d \
./Worker.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


