# INPUTS
HOSTMEM ?= 0
MEASURE_LATENCY ?= 0
KERNEL_NAME ?= fds
GENERATOR_PAR ?= 1
PREDICTOR_PAR ?= 1
DRAINER_PAR ?= 1


################################################################################
#
# Environment
#
################################################################################
FSPX_INCLUDE := $(HOME)/FSPX/include
VITIS_INCLUDE := $(XILINX_VITIS)/include
VIVADO_INCLUDE := $(XILINX_VIVADO)/include
XRT_INCLUDE := $(XILINX_XRT)/include


################################################################################
#
# Libraries
#
################################################################################
XRT_LIB := $(XILINX_XRT)/lib
XRT_LIB_O := $(XRT_LIB)lnx64.o
CXX_LIBS := -lOpenCL -pthread -lrt -lstdc++


################################################################################
#
# Target, Device, Platform
#
################################################################################
TARGET ?= sw_emu
DEVICE ?= u50
PLATFORM ?= xilinx_$(DEVICE)_gen3x16_xdma_5_202210_1
PLATFORM_REPO_PATHS ?= /opt/xilinx/platforms
PFM := $(PLATFORM_REPO_PATHS)/$(PLATFORM)/$(PLATFORM).xpfm


################################################################################
#
# Directories
#
################################################################################
KERNELS_DIR := ./kernels
KERNEL_DIR := ${KERNEL_NAME}${GENERATOR_PAR}${PREDICTOR_PAR}${DRAINER_PAR}
OUT_DIR = $(KERNELS_DIR)/${KERNEL_DIR}/$(TARGET)
TEMP_DIR := $(OUT_DIR)/temp
LOG_DIR := $(OUT_DIR)/log
REPORT_DIR := $(OUT_DIR)/report
BUILD_DIR := $(OUT_DIR)/build

################################################################################
#
# Vitis Compiler (v++)
#
################################################################################
VPP = v++
VPP_INCLUDES := -I$(FSPX_INCLUDE) -I. -Icommon -Idevice -Idevice/includes -Idevice/nodes
VPP_FLAGS := --platform $(PFM) --target $(TARGET)
VPP_FLAGS += --debug
VPP_FLAGS += --save-temps
VPP_FLAGS += --temp_dir $(TEMP_DIR)
VPP_FLAGS += --log_dir $(LOG_DIR)
VPP_FLAGS += --report_dir $(REPORT_DIR)
VPP_PACKAGE_FLAGS := --package.out_dir $(BUILD_DIR)


################################################################################
#
# Kernel names and source files
#
################################################################################

# COMPUTE
COMPUTE_KERNEL_NAME := compute
COMPUTE_CPP := device/$(COMPUTE_KERNEL_NAME).cpp
COMPUTE_XO := $(BUILD_DIR)/$(COMPUTE_KERNEL_NAME).xo


# BITSTREAM
KERNEL_LINK_XCLBIN := $(BUILD_DIR)/$(KERNEL_NAME).link.xclbin
KERNEL_XCLBIN := $(BUILD_DIR)/$(KERNEL_NAME).xclbin


################################################################################
#
# Compute Units (nk), Stream Channels (sc) and Stream Ports (sp)
#
################################################################################
GENERATE_SP_FUNCTION := generate_sp
ifeq ($(HOSTMEM), 1)
	GENERATE_SP_FUNCTION := generate_sp_host
endif


# COMPUTE
COMPUTE_NK := "--connectivity.nk=$(COMPUTE_KERNEL_NAME):1:$(COMPUTE_KERNEL_NAME)_0"
COMPUTE_COMPILE_FLAGS := $(COMPUTE_NK) -DGENERATOR_PAR=${GENERATOR_PAR} -DPREDICTOR_PAR=${PREDICTOR_PAR} -DDRAINER_PAR=${DRAINER_PAR} 
COMPUTE_LINK_FLAGS := $(COMPUTE_COMPILE_FLAGS)


################################################################################
#
# C++ Host
#
################################################################################
CXX = g++
CXX_FLAGS = -std=c++17
CXX_FLAGS += -Wall -Wextra -fmessage-length=0 -Wno-unknown-pragmas -Wno-unused-label -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable

ifdef DEBUG
CXX_FLAGS += -g
else
CXX_FLAGS += -O3 -DNDEBUG -finline-functions  -DWF_RELEASE
endif

ifeq ($(HOSTMEM), 1)
	CXX_FLAGS += -DWF_HOSTMEM
endif

ifeq ($(MEASURE_LATENCY), 1)
	CXX_FLAGS += -DMEASURE_LATENCY
endif

# CXX_FLAGS += -fsanitize=address
# CXX_FLAGS += -fsanitize=thread
# CXX_FLAGS += -fno-omit-frame-pointer -fsanitize-address-use-after-scope -fsanitize=undefined -fno-sanitize-recover=all -fstack-protector -fstack-clash-protection -fPIE -pie -Wl,-z,relro,-z,now -Wl,-z,noexecstack -Wl,-z,defs -Wl,-z,combreloc
CXX_LDFLAGS = -L$(XRT_LIB) -L$(XRT_LIB_O) $(CXX_LIBS)
CXX_INCLUDES = -Icommon -Ihost/includes -I$(FSPX_INCLUDE) -I$(VIVADO_INCLUDE) -I$(XRT_INCLUDE) -I$(VITIS_INCLUDE)

.PHONY: all clean cleanall emconfig bitstream host

emconfig:emconfig.json
emconfig.json:
	emconfigutil --platform $(PLATFORM)

host: host/host.cpp
	mkdir -p bin
	$(CXX) $(CXX_FLAGS) $(CXX_INCLUDES) -o bin/host $< $(CXX_LDFLAGS)


$(COMPUTE_XO): $(COMPUTE_CPP)
	$(VPP) --kernel $(COMPUTE_KERNEL_NAME) $(VPP_FLAGS) $(VPP_INCLUDES) $(COMPUTE_COMPILE_FLAGS) -c -o $@ $<


bitstream: $(COMPUTE_XO)
	$(VPP) --link $(VPP_FLAGS) -o $(KERNEL_XCLBIN) $(+)

all: emconfig $(KERNEL_XCLBIN) host

clean:
	$(RM) -rf _x .run .Xil *.ltx *.log *.info *compile_summary* vitis_analyzer* *link_summary* *.o host

cleanall:
	$(RM) -rf host *.o *.xo _x .run .Xil *.xclbin *.ltx *.log *.info *compile_summary* vitis_analyzer* *link_summary* *.o emconfig.json device_trace_0.csv *.xclbin.package_summary opencl_trace.csv xrt.run_summary summary.csv .ipcache waveform_prefs.json