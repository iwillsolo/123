# ─────────────────────────────────────────────────────────────────────────────
# rtdump-il2cpp-ps5  –  Makefile
#
# Requirements
# ────────────
#  • PS5_PAYLOAD_SDK pointing at the installed SDK, e.g.:
#      export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
#
#  • Install the prebuilt SDK (quickest):
#      wget https://github.com/ps5-payload-dev/pacbrew-repo/releases/latest/download/ps5-payload-dev.tar.gz
#      sudo tar xf ps5-payload-dev.tar.gz -C /
#
#  • Then build libcxx (C++ stdlib – one-time):
#      wget https://raw.githubusercontent.com/ps5-payload-dev/sdk/master/libcxx.sh
#      sudo apt-get install -y cmake ninja-build
#      sudo -E PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk bash libcxx.sh
#
# Build
# ─────
#   make
#
# Deploy  (sends the ELF to the PS5 payload loader)
# ──────
#   make send PS5_HOST=192.168.1.x
# ─────────────────────────────────────────────────────────────────────────────

# Makefile.inc lives at the SDK root (not in a toolchain/ subdirectory).
include $(PS5_PAYLOAD_SDK)/Makefile.inc

TARGET  := rtdump-il2cpp-ps5.elf
SRCDIR  := src

# ── Source files ──────────────────────────────────────────────────────────────
SRCS := $(SRCDIR)/main.cpp             \
        $(SRCDIR)/sig.cpp              \
        $(SRCDIR)/il2cpp_api.cpp       \
        $(SRCDIR)/il2cpp_dump.cpp      \
        $(SRCDIR)/il2cpp_struct_gen.cpp

OBJS := $(SRCS:.cpp=.o)

# ── Compiler flags ────────────────────────────────────────────────────────────
# -I$(SRCDIR)  so that #include "stdafx.h" etc. work without path prefix.
CXXFLAGS += -std=c++17        \
            -O2               \
            -Wall             \
            -Wextra           \
            -fPIC             \
            -I$(SRCDIR)       \
            -I$(PS5_PAYLOAD_SDK)/include

# ── Linker flags ──────────────────────────────────────────────────────────────
# -lc++ / -lc++abi : LLVM libc++ (installed via libcxx.sh)
# -lkernel          : SCE libkernel stubs (sceKernelGetModuleList, Dlsym, …)
# -lc               : C runtime from the SDK
LIBS    := -lc++ -lc++abi -lkernel -lc

# ─────────────────────────────────────────────────────────────────────────────
.PHONY: all clean send

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
	@echo "Built: $@  ($(shell wc -c < $@) bytes)"

$(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

# ── Quick deploy via netcat ───────────────────────────────────────────────────
# Usage:  make send PS5_HOST=192.168.1.42
PS5_HOST ?= 192.168.1.1
PS5_PORT ?= 9021

send: $(TARGET)
	@echo "Sending $(TARGET) to $(PS5_HOST):$(PS5_PORT)…"
	nc -q 1 $(PS5_HOST) $(PS5_PORT) < $(TARGET)
