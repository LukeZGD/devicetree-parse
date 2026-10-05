TARGETS := devicetree-parse devicetree-repack

DEBUG ?= 0
UNAME_S := $(shell uname -s)

HEADERS := $(wildcard *.h)

CFLAGS  := -O2 -Wall
LDFLAGS :=
DEFINES :=
LIBS :=
FRAMEWORKS :=

ifneq ($(DEBUG),0)
DEFINES += -DDEBUG=$(DEBUG)
endif


# ============================================================
# macOS
# ============================================================

ifeq ($(UNAME_S),Darwin)

SDK     ?= macosx
ARCHS   ?= x86_64 arm64

SYSROOT := $(shell xcrun --sdk $(SDK) --show-sdk-path 2>/dev/null)

ifeq ($(SYSROOT),)
$(error Could not find macOS SDK "$(SDK)")
endif

CLANG := $(shell xcrun --sdk $(SDK) --find clang)

CC := $(CLANG) -isysroot $(SYSROOT)

ARCH_FLAGS := $(foreach arch,$(ARCHS),-arch $(arch))

CFLAGS += $(ARCH_FLAGS) -fobjc-arc

FRAMEWORKS += -framework Foundation
FRAMEWORKS += -framework CoreFoundation


# ------------------------------------------------------------
# macOS targets
# ------------------------------------------------------------

devicetree-parse: devicetree-parse.o parse.o
	$(CC) $(CFLAGS) $(FRAMEWORKS) $(DEFINES) $(LDFLAGS) -o $@ $^

devicetree-repack: repack.o
	$(CC) $(CFLAGS) $(FRAMEWORKS) $(DEFINES) $(LDFLAGS) -o $@ $^

devicetree-parse.o: devicetree-parse.c $(HEADERS)
	$(CC) $(CFLAGS) $(FRAMEWORKS) $(DEFINES) $(LDFLAGS) -c -o $@ $<

parse.o: parse.c $(HEADERS)
	$(CC) $(CFLAGS) $(FRAMEWORKS) $(DEFINES) $(LDFLAGS) -c -o $@ $<

repack.o: repack.m $(HEADERS)
	$(CC) $(CFLAGS) $(FRAMEWORKS) $(DEFINES) $(LDFLAGS) -c -o $@ $<


# ============================================================
# Linux
# ============================================================

else ifeq ($(UNAME_S),Linux)

CC ?= gcc

CFLAGS += -D_GNU_SOURCE

LIBS += -lcjson -lm


# ------------------------------------------------------------
# Linux targets
# ------------------------------------------------------------

devicetree-parse: devicetree-parse.o parse.o
	$(CC) $(CFLAGS) $(DEFINES) $(LDFLAGS) -o $@ $^ $(LIBS)

devicetree-repack: repack.o
	$(CC) $(CFLAGS) $(DEFINES) $(LDFLAGS) -o $@ $^ $(LIBS)

devicetree-parse.o: devicetree-parse.c $(HEADERS)
	$(CC) $(CFLAGS) $(DEFINES) $(LDFLAGS) -c -o $@ $<

parse.o: parse.c $(HEADERS)
	$(CC) $(CFLAGS) $(DEFINES) $(LDFLAGS) -c -o $@ $<

repack.o: repack.c $(HEADERS)
	$(CC) $(CFLAGS) $(DEFINES) $(LDFLAGS) -c -o $@ $<


# ============================================================
# Unsupported platform
# ============================================================

else

$(error Unsupported operating system: $(UNAME_S))

endif


# ============================================================
# Common targets
# ============================================================

.PHONY: all clean

all: $(TARGETS)

clean:
	rm -f -- *.o $(TARGETS)
