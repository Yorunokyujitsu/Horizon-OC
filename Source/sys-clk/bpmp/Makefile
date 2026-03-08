# Configuration
PROJECT_NAME = firmware
CPU = arm7tdmi
ARCH = armv4t
OPTIMIZATION = -Ofast

# Toolchain
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

# Directories
SRC_DIR = src
BUILD_DIR = build

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SOURCES))

# Output files
ELF = $(PROJECT_NAME).elf
BIN = $(PROJECT_NAME).bin

# Compiler flags
CFLAGS = -mcpu=$(CPU) -march=$(ARCH) \
         $(OPTIMIZATION) -Wall -ffunction-sections -fdata-sections \
         -I$(SRC_DIR)

# Linker flags
LDFLAGS = -mcpu=$(CPU) -march=$(ARCH) \
          -T linker.ld -Wl,--gc-sections

# Targets
.PHONY: all clean

all: $(BIN)
	@echo "✓ Built: $(BIN)"
	@$(SIZE) $(ELF)

$(BIN): $(ELF)
	@$(OBJCOPY) -O binary $(ELF) $(BIN)

$(ELF): $(OBJECTS)
	@echo "Linking..."
	@$(CC) $(LDFLAGS) $(OBJECTS) -o $(ELF)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	@echo "Compiling: $<"
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR) $(ELF) $(BIN)
	@echo "✓ Cleaned"