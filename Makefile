# Project Name
TARGET = desktop-synthesizer

# Sources
CPP_SOURCES = desktop-synthesizer.cpp oscillator.cpp adsr.cpp moogladder.cpp

# Library Locations
LIBDAISY_DIR = ../DaisyExamples/libDaisy/
DAISYSP_DIR = ../DaisyExamples/DaisySP/

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
