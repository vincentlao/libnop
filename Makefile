what_to_build:: all

-include local.mk

TOOLCHAIN ?=

HOST_OS := $(shell uname -s)

GTEST_INSTALL ?= /opt/local
GTEST_LIB ?= $(GTEST_INSTALL)/lib
GTEST_INCLUDE ?= $(GTEST_INSTALL)/include

HOST_CFLAGS := -g -O2 -Wall -Werror -Wextra -Iinclude
HOST_CXXFLAGS := -std=c++14
HOST_LDFLAGS :=

OUT := out
OUT_HOST_OBJ := $(OUT)/host-obj

ALL :=
DEPS :=

M_NAME := test
M_CFLAGS := -I$(GTEST_INCLUDE) --coverage -O0 -g
M_LDFLAGS := -L$(GTEST_LIB) -lgtest -lgmock
M_OBJS := \
	test/nop_tests.o \
	test/encoding_tests.o \
	test/serializer_tests.o \
	test/utility_tests.o \
	test/variant_tests.o \
	test/handle_tests.o \
	test/thread_local_tests.o \
	test/enum_flags_tests.o \
	test/sip_hash_tests.o \
	test/interface_tests.o \
	test/fungible_tests.o \
	test/optional_tests.o \
	test/result_tests.o \
	test/endian_tests.o \

include build/host-executable.mk

M_NAME := stream_example
M_OBJS := \
	examples/stream.o

include build/host-executable.mk

M_NAME := simple_protocol_example
M_OBJS := \
	examples/simple_protocol.o

include build/host-executable.mk

M_NAME := interface_example
M_OBJS := \
	examples/interface.o

include build/host-executable.mk

M_NAME := pipe_example
M_OBJS := \
	examples/pipe.o

include build/host-executable.mk

M_NAME := table_example
M_OBJS := \
	examples/table.o

include build/host-executable.mk

M_NAME := variant_example
M_OBJS := \
	examples/variant.o

include build/host-executable.mk

clean::
	@echo clean
	@rm -rf $(OUT)

# Generate coverage report with lcov and genhtml. A bit hacky but works okay.
$(OUT)/coverage.info: $(OUT)/test
	$(QUIET)find $(OUT) -name "*.gcda" -exec rm {} \+
	$(QUIET)$(OUT)/test
	lcov --capture --directory $(OUT_HOST_OBJ)/test/test/ --output-file $@ --no-external --base-directory .
	mkdir -p $(OUT)/coverage
	genhtml -o $(OUT)/coverage $@

coverage:: $(OUT)/coverage.info

all:: $(ALL)

# we generate .d as a side-effect of compiling. override generic rule:
%.d:
-include $(DEPS)
