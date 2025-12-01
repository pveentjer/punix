# ------------------------------------------------------------
# Munix 0.001 build system
# ------------------------------------------------------------

CC      := gcc
LD      := ld
OBJCOPY := objcopy
NASM    := nasm
QEMU    := qemu-system-i386

CFLAGS  := -ffreestanding -nostdlib -m32 -O2
LDFLAGS := -m elf_i386

# ------------------------------------------------------------
# Tool verification
# ------------------------------------------------------------
REQUIRED_TOOLS := $(CC) $(LD) $(OBJCOPY) $(NASM) $(QEMU)

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

all: check-tools disk.img

bootsector.bin: bootsector.asm
	$(NASM) -f bin $< -o $@

loader.bin: loader.asm
	$(NASM) -f bin $< -o $@

kernel.bin: kernel.c linker.ld
	$(CC) $(CFLAGS) -c kernel.c -o kernel.o
	$(LD) $(LDFLAGS) -T linker.ld -o kernel.elf kernel.o
	$(OBJCOPY) -O binary kernel.elf kernel.bin

disk.img: bootsector.bin loader.bin kernel.bin
	dd if=/dev/zero of=disk.img bs=512 count=2880 2>/dev/null
	dd if=bootsector.bin of=disk.img conv=notrunc 2>/dev/null
	dd if=loader.bin of=disk.img bs=512 seek=1 conv=notrunc 2>/dev/null
	dd if=kernel.bin of=disk.img bs=512 seek=3 conv=notrunc 2>/dev/null
	@echo "disk.img successfully built."

run: disk.img
	$(QEMU) -drive format=raw,file=disk.img

clean:
	rm -f *.bin *.elf *.o disk.img

.PHONY: all check-tools clean run

