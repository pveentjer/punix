# ------------------------------------------------------------
# Munix 0.001 - 32-bit protected mode build
# ------------------------------------------------------------

CC      := gcc
LD      := ld
OBJCOPY := objcopy
NASM    := nasm
QEMU    := qemu-system-i386

CFLAGS  := -ffreestanding -nostdlib -m32 -O2 -Wall -Wextra -Ikernel
LDFLAGS := -m elf_i386

BUILD_DIR  := build
BOOT_DIR   := boot
KERNEL_DIR := kernel

REQUIRED_TOOLS := $(CC) $(LD) $(OBJCOPY) $(NASM) $(QEMU)

.PHONY: all check-tools clean run help

# ------------------------------------------------------------
# Default target
# ------------------------------------------------------------
all: check-tools $(BUILD_DIR)/disk.img

# ------------------------------------------------------------
# Tool verification
# ------------------------------------------------------------
check-tools:
	@missing=""; \
	for t in $(REQUIRED_TOOLS); do \
	    if ! command -v $$t >/dev/null 2>&1; then \
	        echo "Missing tool: $$t"; \
	        missing="yes"; \
	    fi; \
	done; \
	if [ "$$missing" = "yes" ]; then \
	    echo ""; \
	    echo "Please install the missing tools before building."; \
	    echo "For Fedora:"; \
	    echo "  sudo dnf install gcc binutils nasm qemu-system-x86"; \
	    echo ""; \
	    exit 1; \
	else \
	    echo "All required tools found."; \
	fi

# ------------------------------------------------------------
# Build rules
# ------------------------------------------------------------
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# --- Boot sector and loader ---
$(BUILD_DIR)/bootsector.bin: $(BOOT_DIR)/bootsector.asm | $(BUILD_DIR)
	$(NASM) -f bin $< -o $@

$(BUILD_DIR)/loader.bin: $(BOOT_DIR)/loader.asm | $(BUILD_DIR)
	$(NASM) -f bin $< -o $@

# --- Kernel sources ---
$(BUILD_DIR)/kernel.o: $(KERNEL_DIR)/kernel.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/io.o: $(KERNEL_DIR)/io.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@	

$(BUILD_DIR)/interrupt.o: $(KERNEL_DIR)/interrupt.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@	

$(BUILD_DIR)/vga.o: $(KERNEL_DIR)/vga.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/sched.o: $(KERNEL_DIR)/sched.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/sched_x86.o: $(KERNEL_DIR)/sched_x86.asm | $(BUILD_DIR)
	$(NASM) -f elf32 $< -o $@

$(BUILD_DIR)/process0.o: $(KERNEL_DIR)/process0.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/process1.o: $(KERNEL_DIR)/process1.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/process2.o: $(KERNEL_DIR)/process2.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@



$(BUILD_DIR)/kernel.bin: \
	$(BUILD_DIR)/kernel.o \
	$(BUILD_DIR)/vga.o \
	$(BUILD_DIR)/sched.o \
	$(BUILD_DIR)/io.o \
	$(BUILD_DIR)/interrupt.o \
	$(BUILD_DIR)/process0.o \
	$(BUILD_DIR)/process1.o \
	$(BUILD_DIR)/process2.o \
	$(BUILD_DIR)/sched_x86.o \
	linker.ld | $(BUILD_DIR)
	$(LD) $(LDFLAGS) -T linker.ld -o $(BUILD_DIR)/kernel.elf \
	    $(BUILD_DIR)/kernel.o \
	    $(BUILD_DIR)/vga.o \
	    $(BUILD_DIR)/sched.o \
	    $(BUILD_DIR)/io.o \
	    $(BUILD_DIR)/interrupt.o \
	    $(BUILD_DIR)/process0.o \
		$(BUILD_DIR)/process1.o \
		$(BUILD_DIR)/process2.o \
	    $(BUILD_DIR)/sched_x86.o
	$(OBJCOPY) -O binary $(BUILD_DIR)/kernel.elf $(BUILD_DIR)/kernel.bin

# --- Disk image ---
$(BUILD_DIR)/disk.img: $(BUILD_DIR)/bootsector.bin $(BUILD_DIR)/loader.bin $(BUILD_DIR)/kernel.bin
	dd if=/dev/zero of=$@ bs=512 count=2880 2>/dev/null
	dd if=$(BUILD_DIR)/bootsector.bin of=$@ conv=notrunc 2>/dev/null
	dd if=$(BUILD_DIR)/loader.bin     of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	dd if=$(BUILD_DIR)/kernel.bin     of=$@ bs=512 seek=3 conv=notrunc 2>/dev/null
	@echo "disk.img successfully built in $(BUILD_DIR)/"

# ------------------------------------------------------------
# Utility targets
# ------------------------------------------------------------
run: $(BUILD_DIR)/disk.img
	$(QEMU) -drive format=raw,file=$(BUILD_DIR)/disk.img

clean:
	rm -rf $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)

help:
	@echo "Available make targets:"
	@grep -E '^[a-zA-Z0-9_.-]+:([^=]|$$)' Makefile | \
	    grep -v '^_' | \
	    awk -F':' '{print "  " $$1}'
