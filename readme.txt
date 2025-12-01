Assembly both stages

nasm -f bin bootsector.asm -o bootsector.bin
nasm -f bin loader.asm     -o loader.bin

Build a disk image

# 1. Create a 1.44MB floppy image filled with zeros
dd if=/dev/zero of=disk.img bs=512 count=2880

# 2. Put the boot sector at sector 0
dd if=bootsector.bin of=disk.img conv=notrunc

# 3. Put the loader at sector 1 (sector index 1 = second sector)
dd if=loader.bin of=disk.img bs=512 seek=1 conv=notrunc

Run in QEMU

qemu-system-i386 -fda disk.img
# or:
qemu-system-i386 -drive format=raw,file=disk.img


