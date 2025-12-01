Assembly both stages

nasm -f bin bootsector.asm -o bootsector.bin
nasm -f bin loader.asm -o loader.bin
nasm -f bin kernel.asm -o kernel.bin

Build a disk image

dd if=/dev/zero of=disk.img bs=512 count=2880

# Stage 1 at sector 0
dd if=bootsector.bin of=disk.img conv=notrunc

# Stage 2 (loader) at sector 1 (2 × 512 bytes)
dd if=loader.bin of=disk.img bs=512 seek=1 conv=notrunc

# Stage 3 (kernel) at sector 3 (2 × 512 bytes)
dd if=kernel.bin of=disk.img bs=512 seek=3 conv=notrunc

Run in QEMU

qemu-system-i386 -drive format=raw,file=disk.img


