# You must configure INTELFPGAOCLSDKROOT to point the root directory of the
# Intel(R) FPGA SDK for OpenCL(TM) software installation.
# See http://www.altera.com/literature/hb/opencl-sdk/aocl_getting_started.pdf

# Where is the Intel(R) FPGA SDK for OpenCL(TM) software?
ifeq ($(wildcard $(INTELFPGAOCLSDKROOT)),)
$(error Set INTELFPGAOCLSDKROOT to the directory of the Intel(R) FPGA SDK for OpenCL)
endif
ifeq ($(wildcard $(INTELFPGAOCLSDKROOT)/host/include/CL/opencl.h),)
$(error Set INTELFPGAOCLSDKROOT to the directory of the Intel(R) FPGA SDK for OpenCL)
endif

# ------------------------------------------------------------------------------
# Host
# ------------------------------------------------------------------------------
CXX := arm-linux-gnueabihf-g++
CXXFLAGS = --std=c++11 -pedantic -Wall -Wextra

# Host Files
INCS := $(wildcard includes/*.hpp)
SRCS := $(wildcard host/*.cpp)
LIBS := rt pthread

# Host Directories
INC_DIRS := common includes util includes/rapidjson/include
LIB_DIRS :=

# Target
TARGET_HOST := host
TARGET_HOST_DIR := bin

# ------------------------------------------------------------------------------
# Device
# ------------------------------------------------------------------------------
AOC := aoc
AOC_FLAGS := -v -report
AOC_BOARD := -board=a10s_ddr
#TODO: check if linux64/lib is needed
# OpenCL compile and link flags.
AOCL_COMPILE_CONFIG := $(shell aocl compile-config)
AOCL_LINK_CONFIG := -L/opt/intelFPGA_pro/19.1/hld/board/terasic/a10s/arm32/lib \
					-L/opt/intelFPGA_pro/19.1/hld/host/arm32/lib \
					-L/opt/intelFPGA_pro/19.1/hld/host/linux64/lib \
					-Wl,--no-as-needed -lalteracl -lintel_a10soc32_mmd \
					-lstdc++ -lelf -lacl_emulator_kernel_rt

# Device Files
DEVICE_CODE := device
DEVICE_CODE_DIR := device
DEVICE_INCLUDES :=

# Target
TARGET_DEVICE := device
TARGET_DEVICE_DIR := bin


# ------------------------------------------------------------------------------
# Emulator
# ------------------------------------------------------------------------------
ifeq ($(EMULATOR),1)
# Host compiler
CXX := g++
TARGET_HOST_DIR = bin/emulator

# Device Compiler
AOC_FLAGS += -emulator-channel-depth-model=strict
AOC_BOARD := -march=emulator
AOCL_LINK_CONFIG := $(shell aocl link-config) -lalteracl -lacl_emulator_kernel_rt
TARGET_DEVICE_DIR := bin/emulator

#_LD_LIBRARY_PATH := $(LD_LIBRARY_PATH):$(INTELFPGAOCLSDKROOT)/host/linux64/lib
endif


# ------------------------------------------------------------------------------
# Fast-compile, Debug and Verbose
# ------------------------------------------------------------------------------
ifeq ($(FASTCOMPILE),1)
AOC_FLAGS += -fast-compile
else
endif

ifeq ($(DEBUG),1)
CXXFLAGS += -g
AOC_FLAGS += -g -profile=all
else
CXXFLAGS += -O3
AOC_FLAGS += -g0
endif

ifeq ($(VERBOSE),1)
ECHO :=
else
ECHO := @
endif


.PHONY: host device report cleanhost cleandevice clean

$(shell mkdir -p $(TARGET_HOST_DIR) $(TARGET_DEVICE_DIR))

#host: LD_LIBRARY_PATH := $(_LD_LIBRARY_PATH)
host: $(SRCS) $(INCS) $(TARGET_HOST_DIR)
	$(ECHO)$(CXX) $(CXXFLAGS) \
		$(foreach D,$(INC_DIRS),-I$D) \
		$(foreach D,$(DEVICE_INCLUDES),-I$D) \
		$(AOCL_COMPILE_CONFIG) \
		$(SRCS) \
		$(AOCL_LINK_CONFIG) \
		$(foreach L,$(LIBS),-l$L) \
		-o $(TARGET_HOST_DIR)/$(TARGET_HOST)

device: $(DEVICE_CODE_DIR)/$(DEVICE_CODE).cl $(TARGET_DEVICE_DIR)
	$(ECHO)$(AOC) $(AOC_BOARD) $(AOC_FLAGS) \
		$(DEVICE_CODE_DIR)/$(DEVICE_CODE).cl \
		$(foreach D,$(DEVICE_INCLUDES),-I$D) \
		-o $(TARGET_DEVICE_DIR)/$(TARGET_DEVICE).aocx

%.aocx: $(TARGET_DEVICE_DIR)
	$(ECHO)$(AOC) $(AOC_BOARD) $(AOC_FLAGS) \
		$(DEVICE_CODE_DIR)/$*.cl \
		$(foreach D,$(DEVICE_INCLUDES),-I$D) \
		-o $(TARGET_DEVICE_DIR)/$*.aocx

report: $(DEVICE_CODE_DIR)/$(DEVICE_CODE).cl
	$(ECHO)rm -rf $(TARGET_DEVICE_DIR)/$(TARGET_DEVICE)
	$(ECHO)$(AOC) -rtl -report\
		$(DEVICE_CODE_DIR)/$(DEVICE_CODE).cl \
		$(foreach D,$(DEVICE_INCLUDES),-I$D) \
		-o $(TARGET_DEVICE_DIR)/$(TARGET_DEVICE)
	$(ECHO)echo "You can find the report at $(TARGET_DEVICE_DIR)/$(TARGET_DEVICE)/reports/report.html"

cleanhost :
	$(ECHO)rm -f $(TARGET_HOST_DIR)/$(TARGET_HOST)

cleandevice :
	$(ECHO)rm -rf $(TARGET_DEVICE_DIR)/$(DEVICE_CODE)
	$(ECHO)rm -f $(TARGET_DEVICE_DIR)/$(DEVICE_CODE).aoco
	$(ECHO)rm -f $(TARGET_DEVICE_DIR)/$(DEVICE_CODE).aocr
	$(ECHO)rm -if $(TARGET_DEVICE_DIR)/$(DEVICE_CODE).aocx

clean : cleanhost cleandevice
