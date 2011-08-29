################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: libMPOST_Linux.a libMPOST_Linux.so

# Tool invocations
libMPOST_Linux.a: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC Archiver'
	$(CXX) -rdynamic -shared -fPIC -o libMPOST_Linux.so $(OBJS) $(USER_OBJS) $(LIBS)
	ar -r "libMPOST_Linux.a" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

libMPOST_Linux.so: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C++ Linker'
	$(CXX) -rdynamic -shared -fPIC -o $@ $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJS)$(C++_DEPS)$(C_DEPS)$(CC_DEPS)$(ARCHIVES)$(CPP_DEPS)$(CXX_DEPS)$(C_UPPER_DEPS) libMPOST_Linux.a libMPOST_Linux.so
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets
