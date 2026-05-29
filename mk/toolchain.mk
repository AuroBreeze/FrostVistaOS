# Toolchain and host tool selection.
#
# Consumes:
#   ARCH, CROSS
#
# Produces:
#   CROSS, CC, DUMP, HOST_CC, XXD

CC     = $(CROSS)-gcc
DUMP   = $(CROSS)-objdump
HOST_CC = gcc
XXD ?= xxd
