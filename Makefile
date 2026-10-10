# --- Wii U Plugin Build Environment Configuration ---

# Ensure devkitPPC and the Wii U Toolchain variables are set correctly
ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. Export it in your ~/.bashrc or system variables.")
endif

include $(DEVKITPRO)/wut/share/wut_rules

# --- Project Metadata Setup ---
TARGET          := WiiUKioskRecentApps
BUILD           := build
SOURCE          := .
DATA            := data
INCLUDES        := 

# --- Compiler Configuration Flags ---
# Includes standard Wii U kernel and Aroma plugin optimization flags
CFLAGS          := -g -O2 -Wall -mrvl -mcpu=750 -meabi -mhard-float
CXXFLAGS        := $(CFLAGS) -fno-rtti -fno-exceptions

# Linker library paths targeting the Wii U Plugin System (WUPS)
LIBS            := -lwups -lwhb -lcoreinit -lsysapp

# --- Build Execution Pipeline Rules ---
.PHONY: all clean

all: $(TARGET).wps

$(TARGET).wps: $(TARGET).elf
	@echo "Packaging Aroma plugin file: $@"
	@wupscreateplugin $< $@

$(TARGET).elf: main.o
	@echo "Linking system binaries: $@"
	@$(CXX) $(CXXFLAGS) -melf $(LDFLAGS) $^ $(LIBS) -o $@

%.o: %.cpp
	@echo "Compiling source files: $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@echo "Cleaning up temporary object artifacts..."
	@rm -f *.o *.elf *.wps
