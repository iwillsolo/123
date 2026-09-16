PS5_HOST ?= ps5
PS5_PORT ?= 9021

ifndef PS5_PAYLOAD_SDK
$(error PS5_PAYLOAD_SDK is undefined)
endif

include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk

TARGET := rtdump-il2cpp-ps5.elf
SRCDIR := src

SRCS := $(SRCDIR)/main.cpp \
        $(SRCDIR)/sig.cpp \
        $(SRCDIR)/il2cpp_api.cpp \
        $(SRCDIR)/il2cpp_dump.cpp \
        $(SRCDIR)/il2cpp_struct_gen.cpp

OBJS := $(SRCS:.cpp=.o)

CXXFLAGS += -std=c++17 \
            -O2 \
            -Wall \
            -Wextra \
            -fPIC \
            -I$(SRCDIR)

LIBS := -lc++ -lc++abi -lkernel -lc

.PHONY: all clean send

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LIBS)

$(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

send: $(TARGET)
	@echo "Sending $(TARGET) to $(PS5_HOST):$(PS5_PORT)..."
	$(PS5_DEPLOY) -h $(PS5_HOST) -p $(PS5_PORT) $(TARGET)