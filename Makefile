TARGETS := devicetree-parse devicetree-repack

DEBUG   ?= 0
UNAME_S := $(shell uname -s)

# Base C flags
CFLAGS  += -O2 -Wall
LDFLAGS +=

ifneq ($(DEBUG),0)
DEFINES += -DDEBUG=$(DEBUG)
endif

# --- macOS Configuration ---
ifeq ($(UNAME_S),Darwin)
	SDK     ?= macosx
	ARCHS   ?= x86_64 arm64
	SYSROOT := $(shell xcrun --sdk $(SDK) --show-sdk-path 2>/dev/null)

	ifeq ($(SYSROOT),)
		$(error Could not find macOS SDK "$(SDK)")
	endif

	CLANG   := $(shell xcrun --sdk $(SDK) --find clang)
	CC      := $(CLANG) -isysroot $(SYSROOT)

	# Format multi-arch build flags (-arch x86_64 -arch arm64)
	ARCH_FLAGS := $(foreach arch,$(ARCHS),-arch $(arch))
	CFLAGS     += $(ARCH_FLAGS) -fobjc-arc
	FRAMEWORKS += -framework Foundation -framework CoreFoundation

	# Source file for repack target on macOS
	REPACK_SRC := repack.m

# --- Linux Configuration ---
else ifeq ($(UNAME_S),Linux)
	CC      ?= gcc
	CFLAGS  += -D_GNU_SOURCE
	LIBS    += -lcjson -lm

	# Source file for repack target on Linux
	REPACK_SRC := repack.c
endif

REPACK_OBJ := $(REPACK_SRC:.c=.o)
REPACK_OBJ := $(REPACK_OBJ:.m=.o)

# Headers list for tracking dependencies
HEADERS := $(wildcard *.h)

.PHONY: all clean

all: $(TARGETS)

# Binary Link Targets
devicetree-parse: devicetree-parse.o parse.o
	$(CC) $(CFLAGS) $(DEFINES) $^ $(LDFLAGS) -o $@

devicetree-repack: $(REPACK_OBJ)
	$(CC) $(CFLAGS) $(DEFINES) $^ $(LDFLAGS) $(FRAMEWORKS) $(LIBS) -o $@

# C Object Compilation Rule
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(DEFINES) -c $< -o $@

# Objective-C Object Compilation Rule (macOS)
%.o: %.m $(HEADERS)
	$(CC) $(CFLAGS) $(DEFINES) -c $< -o $@

clean:
	rm -f -- *.o $(TARGETS)
