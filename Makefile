# Project Name
TARGET = dualie

# Sources
CPP_SOURCES = src/main.cpp src/oscillator.cpp src/adsr.cpp src/moogladder.cpp

# Library Locations
LIBDAISY_DIR = lib/libDaisy/
DAISYSP_DIR = lib/DaisySP/

#ASM_SOURCES = \
$(LIBDAISY_DIR)/Drivers/CMSIS/DSP/Source/TransformFunctions/arm_bitreversal2.s

C_SOURCES = \
$(LIBDAISY_DIR)/Drivers/CMSIS/DSP/Source/FastMathFunctions/arm_sin_f32.c \
$(LIBDAISY_DIR)/Drivers/CMSIS/DSP/Source/BasicMathFunctions/arm_abs_f32.c \
$(LIBDAISY_DIR)/Drivers/CMSIS/DSP/Source/BasicMathFunctions/arm_mult_f32.c \
$(LIBDAISY_DIR)/Drivers/CMSIS/DSP/Source/BasicMathFunctions/arm_add_f32.c \
$(LIBDAISY_DIR)/Drivers/CMSIS/DSP/Source/BasicMathFunctions/arm_scale_f32.c \
$(LIBDAISY_DIR)/Drivers/CMSIS/DSP/Source/BasicMathFunctions/arm_offset_f32.c \
$(LIBDAISY_DIR)/Drivers/CMSIS/DSP/Source/SupportFunctions/arm_fill_f32.c \
$(LIBDAISY_DIR)/Drivers/CMSIS/DSP/Source/SupportFunctions/arm_negate_f32.c \
$(LIBDAISY_DIR)/Drivers/CMSIS/DSP/Source/SupportFunctions/arm_sub_f32.c \
$(LIBDAISY_DIR)/Drivers/CMSIS/DSP/Source/CommonTables/arm_common_tables.c \
$(LIBDAISY_DIR)/Drivers/CMSIS/DSP/Source/SupportFunctions/arm_clip_f32.c

#arm clip added specially

C_INCLUDES += \
-I$(LIBDAISY_DIR)/Drivers/CMSIS/DSP/Include 

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
