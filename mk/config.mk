# User-facing build configuration.
#
# This file must not depend on any other make fragment, generated file, host
# tool, or discovered source list. It is the first include point and should only
# define user-overridable defaults plus compatibility aliases.
#
# Produces:
#   ARCH, TEST, BOOT, FS_LIST, ROOTFS, LOG_NUM, BUILD
#
# Keep this file limited to defaults and compatibility aliases.

# Set the default ARCH to riscv
ARCH ?= riscv
# Set the test file to run
TEST ?= runner
# Set the default BOOT method
# such as bare, opensbi
BOOT ?= bare
# Set the default log level
LOG_NUM ?= 2
# Set the default build type
BUILD ?= release
# Set the enabled filesystems and boot root filesystem.
# FS_LIST is space-separated; ROOTFS must be one of its entries.
FS_LIST ?= easyfs
ROOTFS ?= easyfs
# fs need if rootfs is ext4
EXT4_IMG ?= sdcard-rv.img

CONTEST_MEM ?= 128M
CONTEST_SMP ?= 1

# Out-of-tree build directories
BUILD_DIR := build
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
OBJ_DIR   := $(BUILD_DIR)/obj
GEN_DIR   := $(BUILD_DIR)/gen
TEST_DIR  := $(BUILD_DIR)/test
USER_DIR  := $(BUILD_DIR)/user
# Define the disk image name
DISK_IMG = $(BUILD_DIR)/disk.img

ifeq ($(LOG), TRACE)
	LOG_NUM = 0
else ifeq ($(LOG), DEBUG)
	LOG_NUM = 1
else ifeq ($(LOG), INFO)
	LOG_NUM = 2
else ifeq ($(LOG), WARN)
	LOG_NUM = 3
else ifeq ($(LOG), ERROR)
	LOG_NUM = 4
endif

# Build type: release (optimized) or debug (symbols, no optimization)
ifeq ($(BUILD), debug)
	OPT_FLAGS = -O0 -g
else
	OPT_FLAGS = -O2
endif
