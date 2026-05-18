#*****************************************************************************
# Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#******************************************************************************
#*/

PREFIX ?= .
OBJ_DIR ?= $(PREFIX)/obj
LIB_DIR ?= $(PREFIX)/lib


all:
	mkdir -p $(OBJ_DIR) $(LIB_DIR)
	$(CC) -c $(CFLAGS) -MD -Iinclude src/mylib.c -o $(OBJ_DIR)/mylib.o
	# Force a fresh archive each build. The repo ships libamd.a as a
	# pre-compiled NPCX4 artifact containing the whole AMD app tree;
	# without this rm the `ar r` keeps those NPCX4 objects in and
	# clashes with the same .c files we rebuild from source at link
	# time. Realtek/RTS5918 builds must NOT inherit that pre-built blob.
	rm -f $(LIB_DIR)/libamd.a
	$(AR) -rcs $(LIB_DIR)/libamd.a $(OBJ_DIR)/mylib.o

clean:
	rm -rf $(OBJ_DIR) $(LIB_DIR)