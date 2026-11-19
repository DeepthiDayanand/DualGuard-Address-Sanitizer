# ---------------------------------------------------------------------------
# Runtime library for deltapointers – standalone build with clang-14
# No infra pkg-config dependency.
#
# Variables (override on command line as needed):
#   OBJDIR           output directory for objects and the archive
#   LLVM_BIN         path to LLVM 14 bin/ dir (contains clang, llvm-ar, ...)
#   ADDRSPACE_BITS   address-space bits (default: 32)
#   RUNTIME_STATS    set to 1 to enable runtime stats
#   OVERFLOW_BIT     set to 1 to use the overflow-bit masking variant
#   DEBUG            set to 1 to enable debug logging
# ---------------------------------------------------------------------------

OBJDIR         ?= ../build/runtime
LLVM_BIN       ?= $(HOME)/Spectre/uslh-ext/llvm-project-14.0.0.src/build/bin
ADDRSPACE_BITS ?= 32
RUNTIME_STATS  ?= 0
OVERFLOW_BIT   ?= 0
DEBUG          ?= 0

CC  := $(LLVM_BIN)/clang
AR  := $(LLVM_BIN)/llvm-ar

# Include paths:
#  - runtime/     : addrspace.h, noinstrument.h, source-instrumentation.h
#  - infra runtime: dump-ir-helper.h (infra noinstrument.h)
#  - test dir     : test-side noinstrument.h (has the same NOINSTRUMENT macro)
INCDIRS := -I. \
           -I../infra/llvm-passes/3.8.0/include/runtime \
           -I../deltatags-test

CFLAGS := -fPIC -O2 -Wall -Wextra \
           -DADDRSPACE_BITS=$(ADDRSPACE_BITS) \
           $(INCDIRS)

ifeq ($(OVERFLOW_BIT),1)
	CFLAGS += -DOVERFLOW_BIT
endif
ifeq ($(RUNTIME_STATS),1)
	CFLAGS += -DRUNTIME_STATS
endif
ifeq ($(DEBUG),1)
	CFLAGS += -DDEBUG
endif

LIB  := $(OBJDIR)/libdeltatag_rt.a

# mask.c requires BMI2 for _bzhi_u64 / _pext_u64
MASK_CFLAGS := $(CFLAGS) -mbmi2

SRCS  := debug.c libptrret.c mask-wrappers.c runtimestats.c mask.c
OBJS  := $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))

.PHONY: all clean

all: $(LIB)

$(LIB): $(OBJS) | $(OBJDIR)
	$(AR) rcs $@ $^

# mask.c needs -mbmi2
$(OBJDIR)/mask.o: mask.c addrspace.h | $(OBJDIR)
	$(CC) -c $(MASK_CFLAGS) -o $@ $<

$(OBJDIR)/%.o: %.c addrspace.h | $(OBJDIR)
	$(CC) -c $(CFLAGS) -o $@ $<

$(OBJDIR):
	mkdir -p $@

clean:
	rm -rf $(OBJDIR)
