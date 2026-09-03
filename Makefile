# Makefile for diffmap - cross-platform (Linux x86-64 and macOS ARM64/x86-64)
#
# OpenMP is enabled automatically. Thread count is controlled at runtime
# by the OMP_NUM_THREADS environment variable; if unset the runtime uses
# all available cores (which is the default behaviour).
#
# To run with e.g. 8 threads:   OMP_NUM_THREADS=8 ./diffmap.exe
# To use all cores (default):   ./diffmap.exe

FC      = gfortran
CC      = gcc
FFLAGS  = -w -fallow-argument-mismatch -std=legacy -fopenmp
CFLAGS  = -w -std=c99 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE=1
LDFLAGS = -fopenmp

# Detect OS and architecture
UNAME := $(shell uname -s)
ARCH  := $(shell uname -m)

ifeq ($(UNAME),Linux)
  ifeq ($(ARCH),x86_64)
    FFLAGS += -mcmodel=large
    CFLAGS += -mcmodel=large
  endif
endif

ifeq ($(UNAME),Darwin)
  # Pass the active Xcode SDK path so the linker can find libSystem.
  # On macOS, gfortran uses its own OpenMP runtime (libgomp) -- no
  # extra flags needed beyond -fopenmp.
  SDK := $(shell xcrun --show-sdk-path 2>/dev/null)
  ifneq ($(SDK),)
    LDFLAGS += -isysroot $(SDK)
  endif
endif

TARGET  = diffmap.exe
OBJS    = diffmap.o iof.o ioc.o

$(TARGET): $(OBJS)
	$(FC) $(FFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)
	rm -f $(OBJS)

diffmap.o: diffmap.f
	$(FC) $(FFLAGS) -c diffmap.f

iof.o: iof.f
	$(FC) $(FFLAGS) -c iof.f

ioc.o: ioc.c
	$(CC) $(CFLAGS) -c ioc.c

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: clean

# --------------------------------------------------------------------------
# macOS OpenMP note:
# Apple's system clang does not support -fopenmp out of the box.
# gfortran from Homebrew does, but needs the libomp runtime installed:
#
#   brew install libomp
#   brew install gcc        # if not already installed
#
# If you installed gcc via Homebrew it may be named gcc-14 (or gcc-13 etc).
# Override the compiler on the make command line if needed:
#
#   make FC=gcc-14
#
# To verify OpenMP is actually running, check CPU usage during a run, or:
#   OMP_NUM_THREADS=2 ./diffmap.exe   (should use ~200% CPU)
# --------------------------------------------------------------------------
