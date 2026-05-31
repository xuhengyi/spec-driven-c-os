RISCV_PREFIX ?= /usr/local/riscv64-unknown-elf-gcc/bin/riscv64-unknown-elf-
CC := $(RISCV_PREFIX)gcc
OBJCOPY := $(RISCV_PREFIX)objcopy

BUILD_DIR := build
KERNEL_ELF := $(BUILD_DIR)/cos-ch3.elf
KERNEL_BIN := $(BUILD_DIR)/cos-ch3.bin
APP_ASM := $(BUILD_DIR)/apps.S
LINKER := linker.ld
SRCS := src/entry.S src/main.c $(APP_ASM)

CFLAGS := -std=c11 -O2 -g -Wall -Wextra -ffreestanding -fno-common \
	-fno-pic -fno-pie -fno-stack-protector -mcmodel=medany \
	-march=rv64gc -mabi=lp64
ASFLAGS := $(CFLAGS)
LDFLAGS := -nostdlib -nostartfiles -Wl,-T$(LINKER) -Wl,--gc-sections \
	-Wl,-Map=$(BUILD_DIR)/cos-ch3.map

.PHONY: all run clean

all: $(KERNEL_ELF) $(KERNEL_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(APP_ASM): scripts/build_apps.sh | $(BUILD_DIR)
	bash scripts/build_apps.sh "$(BUILD_DIR)"

$(KERNEL_ELF): $(SRCS) $(LINKER) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(ASFLAGS) $(SRCS) $(LDFLAGS) -o $@

$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

run: $(KERNEL_ELF)
	qemu-system-riscv64 -machine virt -nographic -bios default -kernel $(KERNEL_ELF)

clean:
	rm -rf $(BUILD_DIR)
