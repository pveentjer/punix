cmake_minimum_required(VERSION 3.20)

project(PUnix C ASM)

# ------------------------------------------------------------
# Tool detection
# ------------------------------------------------------------
find_program(NASM_EXECUTABLE nasm REQUIRED)
find_program(OBJCOPY_EXECUTABLE objcopy REQUIRED)
find_program(LD_EXECUTABLE ld REQUIRED)
find_program(QEMU_EXECUTABLE qemu-system-i386 REQUIRED)

# ------------------------------------------------------------
# Architecture selection (only x86 supported for now)
# ------------------------------------------------------------
set(ARCH "x86" CACHE STRING "Target architecture")

if (NOT ARCH STREQUAL "x86")
    message(FATAL_ERROR "Unsupported ARCH=${ARCH}. Only ARCH=x86 is supported currently.")
endif()

add_compile_definitions(ARCH_X86=1)

# ------------------------------------------------------------
# Compiler / flags
# ------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(COMMON_C_FLAGS "-ffreestanding -nostdlib -m32 -O2 -Wall -Wextra")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_C_FLAGS}")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/arch/${ARCH}/include)

# ------------------------------------------------------------
# Directory layout
# ------------------------------------------------------------
set(BOOT_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/arch/${ARCH}/boot)
set(KERNEL_DIR      ${CMAKE_CURRENT_SOURCE_DIR}/kernel)
set(BIN_DIR         ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(LIBS_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/libs)
set(ARCH_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/arch)
set(FS_DIR          ${CMAKE_CURRENT_SOURCE_DIR}/fs)
set(BUILD_DIR       ${CMAKE_CURRENT_BINARY_DIR})

# Arch-derived paths
set(BOOT_ARCH_DIR   ${BOOT_DIR}/arch/${ARCH})
set(ARCH_SRC_DIR    ${ARCH_DIR}/${ARCH})

# ------------------------------------------------------------
# Boot sector and loader
# ------------------------------------------------------------
add_custom_command(
        OUTPUT ${BUILD_DIR}/bootsector.bin
        COMMAND ${NASM_EXECUTABLE} -f bin ${BOOT_DIR}/bootsector.asm -o bootsector.bin
        DEPENDS ${BOOT_DIR}/bootsector.asm
        WORKING_DIRECTORY ${BUILD_DIR}
        COMMENT "Assembling bootsector.bin"
)

add_custom_command(
        OUTPUT ${BUILD_DIR}/loader.bin
        COMMAND ${NASM_EXECUTABLE} -f bin ${BOOT_DIR}/loader.asm -o loader.bin
        DEPENDS ${BOOT_DIR}/loader.asm
        WORKING_DIRECTORY ${BUILD_DIR}
        COMMENT "Assembling loader.bin"
)

# ------------------------------------------------------------
# kpremain.asm -> kpremain_x86.o
# ------------------------------------------------------------
set(PREBOOT_ASM_SRC  ${ARCH_SRC_DIR}/kpremain.asm)
set(PREBOOT_ASM_OBJ  ${BUILD_DIR}/kpremain_${ARCH}.o)

add_custom_command(
        OUTPUT ${PREBOOT_ASM_OBJ}
        COMMAND ${NASM_EXECUTABLE} -f elf32 ${PREBOOT_ASM_SRC} -o ${PREBOOT_ASM_OBJ}
        DEPENDS ${PREBOOT_ASM_SRC}
        WORKING_DIRECTORY ${BUILD_DIR}
        COMMENT "Assembling ${PREBOOT_ASM_OBJ}"
)

# ------------------------------------------------------------
# Collect all binary names and paths
# ------------------------------------------------------------
set(ALL_BINS
        swapper init sh loop ps spawn_chain kill ls cat echo
        printenv tty pwd date uptime clear time
)

set(BIN_PATHS
        "/sbin/swapper"
        "/sbin/init"
        "/bin/sh"
        "/bin/loop"
        "/bin/ps"
        "/bin/spawn_chain"
        "/bin/kill"
        "/bin/ls"
        "/bin/cat"
        "/bin/echo"
        "/bin/printenv"
        "/bin/tty"
        "/bin/pwd"
        "/bin/date"
        "/bin/uptime"
        "/bin/clear"
        "/bin/time"
)

# ------------------------------------------------------------
# Generate embedded_bins.c
# ------------------------------------------------------------
file(WRITE ${BUILD_DIR}/embedded_bins.c "#include \"kernel/kutils.h\"\n")
file(APPEND ${BUILD_DIR}/embedded_bins.c "#include \"kernel/console.h\"\n")
file(APPEND ${BUILD_DIR}/embedded_bins.c "#include \"kernel/elf_loader.h\"\n\n")
file(APPEND ${BUILD_DIR}/embedded_bins.c "/* Auto-generated - do not edit */\n\n")

foreach(name ${ALL_BINS})
    file(APPEND ${BUILD_DIR}/embedded_bins.c "extern unsigned char _binary_${name}_elf_start[];\n")
    file(APPEND ${BUILD_DIR}/embedded_bins.c "extern unsigned char _binary_${name}_elf_end[];\n")
endforeach()

file(APPEND ${BUILD_DIR}/embedded_bins.c "\nconst struct embedded_bin embedded_bins[] = {\n")

list(LENGTH ALL_BINS count)
math(EXPR last "${count} - 1")
foreach(idx RANGE ${last})
    list(GET ALL_BINS ${idx} name)
    list(GET BIN_PATHS ${idx} path)
    file(APPEND ${BUILD_DIR}/embedded_bins.c "    {\"${path}\", _binary_${name}_elf_start, _binary_${name}_elf_end},\n")
endforeach()

file(APPEND ${BUILD_DIR}/embedded_bins.c "};\n\n")
file(APPEND ${BUILD_DIR}/embedded_bins.c "const size_t embedded_bin_count = sizeof(embedded_bins) / sizeof(embedded_bins[0]);\n\n")
file(APPEND ${BUILD_DIR}/embedded_bins.c "
const struct embedded_bin *find_bin(const char *name)
{
    for (size_t i = 0; i < embedded_bin_count; i++)
    {
        if (k_strcmp(embedded_bins[i].name, name) == 0)
        {
            return &embedded_bins[i];
        }
    }
    return NULL;
}
")

# ------------------------------------------------------------
# Kernel core sources
# ------------------------------------------------------------
set(KERNEL_CORE_SOURCES
        ${KERNEL_DIR}/core/console.c
        ${KERNEL_DIR}/core/kernel.c
        ${KERNEL_DIR}/core/sched.c
        ${KERNEL_DIR}/core/syscall.c
        ${KERNEL_DIR}/core/kutils.c
        ${KERNEL_DIR}/core/elf_loader.c
        ${KERNEL_DIR}/core/task_table.c
        ${KERNEL_DIR}/core/tty.c
        ${KERNEL_DIR}/core/wait.c
        ${KERNEL_DIR}/core/mm.c
        ${BUILD_DIR}/embedded_bins.c
        arch/x86/panic.c
)

# ------------------------------------------------------------
# FS sources (moved to top-level fs/)
# ------------------------------------------------------------
set(FS_SOURCES
        ${FS_DIR}/vfs.c
        ${FS_DIR}/files.c
        ${FS_DIR}/bin_fs.c
        ${FS_DIR}/dev_fs.c
        ${FS_DIR}/fs_util.c
        ${FS_DIR}/proc_fs.c
        ${FS_DIR}/sys_fs.c
        ${FS_DIR}/root_fs.c
)

set(KERNEL_SOURCES
        ${KERNEL_CORE_SOURCES}
        ${KERNEL_DRIVER_SOURCES}
        ${FS_SOURCES}
)

# ------------------------------------------------------------
# Arch sources
# ------------------------------------------------------------
set(ARCH_SOURCES
        ${ARCH_SRC_DIR}/irq.c
        ${ARCH_SRC_DIR}/keyboard.c
        ${ARCH_SRC_DIR}/clock.c
        ${ARCH_SRC_DIR}/mm.c
        ${ARCH_SRC_DIR}/console_vga.c
        ${ARCH_SRC_DIR}/console_vesa.c
)

add_library(kernel_objs OBJECT
        ${KERNEL_SOURCES}
        ${ARCH_SOURCES}
)

# ------------------------------------------------------------
# sched.asm -> sched_x86.o
# ------------------------------------------------------------
set(SCHED_ASM_SRC   ${ARCH_SRC_DIR}/sched.asm)
set(SCHED_ASM_OBJ   ${BUILD_DIR}/sched_${ARCH}.o)

add_custom_command(
        OUTPUT ${SCHED_ASM_OBJ}
        COMMAND ${NASM_EXECUTABLE} -f elf32 ${SCHED_ASM_SRC} -o ${SCHED_ASM_OBJ}
        DEPENDS ${SCHED_ASM_SRC}
        WORKING_DIRECTORY ${BUILD_DIR}
        COMMENT "Assembling ${SCHED_ASM_OBJ}"
)

# ------------------------------------------------------------
# syscall.asm -> syscall_x86.o
# ------------------------------------------------------------
set(SYSCALL_ASM_SRC   ${ARCH_SRC_DIR}/syscall.asm)
set(SYSCALL_ASM_OBJ   ${BUILD_DIR}/syscall_${ARCH}.o)

add_custom_command(
        OUTPUT ${SYSCALL_ASM_OBJ}
        COMMAND ${NASM_EXECUTABLE} -f elf32 ${SYSCALL_ASM_SRC} -o ${SYSCALL_ASM_OBJ}
        DEPENDS ${SYSCALL_ASM_SRC}
        WORKING_DIRECTORY ${BUILD_DIR}
        COMMENT "Assembling ${SYSCALL_ASM_OBJ}"
)

# ------------------------------------------------------------
# Userland crt0 (_start) -> crt0_i386.o
# ------------------------------------------------------------
set(CRT0_SRC ${BIN_DIR}/crt0_i386.asm)
set(CRT0_OBJ ${BUILD_DIR}/crt0_i386.o)

add_custom_command(
        OUTPUT ${CRT0_OBJ}
        COMMAND ${NASM_EXECUTABLE} -f elf32 ${CRT0_SRC} -o ${CRT0_OBJ}
        DEPENDS ${CRT0_SRC}
        WORKING_DIRECTORY ${BUILD_DIR}
        COMMENT "Building crt0_i386.o"
)

add_custom_target(crt0 DEPENDS ${CRT0_OBJ})

# ------------------------------------------------------------
# Helper: build & embed a binary
# ------------------------------------------------------------
function(add_bin name)
    add_executable(${name}.elf
            ${CRT0_OBJ}
            ${BIN_DIR}/${name}.c
            ${LIBS_DIR}/libc.c
    )
    add_dependencies(${name}.elf crt0)

    target_compile_options(${name}.elf PRIVATE
            -ffreestanding
            -nostdlib
            -m32
            -O2
            -Wall
            -Wextra
            -ffunction-sections
            -fdata-sections
            -fno-pic
            -fno-pie
            -fno-asynchronous-unwind-tables
    )

    target_link_options(${name}.elf PRIVATE
            -m32
            -nostdlib
            -no-pie
            -Wl,-T,${CMAKE_SOURCE_DIR}/bin.ld
            -Wl,-e,_start
            -Wl,--gc-sections
            -Wl,--build-id=none
            -Wl,--no-dynamic-linker
    )

    set_target_properties(${name}.elf PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR}
    )

    add_custom_command(TARGET ${name}.elf POST_BUILD
            COMMAND ${CMAKE_STRIP} --strip-debug ${BUILD_DIR}/${name}.elf
            COMMAND ${OBJCOPY_EXECUTABLE} --remove-section=.comment ${BUILD_DIR}/${name}.elf
            COMMAND ${OBJCOPY_EXECUTABLE} --remove-section=.note.gnu.property ${BUILD_DIR}/${name}.elf
            COMMAND ${OBJCOPY_EXECUTABLE} --remove-section=.note.GNU-stack ${BUILD_DIR}/${name}.elf
            COMMENT "Stripping ${name}.elf"
    )

    add_custom_command(
            OUTPUT ${BUILD_DIR}/${name}_elf.o
            COMMAND ${OBJCOPY_EXECUTABLE}
            -I binary -O elf32-i386 -B i386
            ${name}.elf ${name}_elf.o
            DEPENDS ${name}.elf
            WORKING_DIRECTORY ${BUILD_DIR}
            COMMENT "Embedding ${name}.elf into kernel"
    )

    set(${name}_OBJ ${BUILD_DIR}/${name}_elf.o PARENT_SCOPE)
endfunction()

# ------------------------------------------------------------
# Build binaries and collect their object files
# ------------------------------------------------------------
set(EMBEDDED_OBJS "")
foreach(bin ${ALL_BINS})
    add_bin(${bin})
    list(APPEND EMBEDDED_OBJS ${${bin}_OBJ})
endforeach()

# ------------------------------------------------------------
# Kernel ELF
# ------------------------------------------------------------
add_executable(kernel.elf
        $<TARGET_OBJECTS:kernel_objs>
        ${PREBOOT_ASM_OBJ}
        ${SCHED_ASM_OBJ}
        ${SYSCALL_ASM_OBJ}
        ${EMBEDDED_OBJS}
)

set_target_properties(kernel.elf PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR}
)

target_link_options(kernel.elf PRIVATE
        -m32
        -nostdlib
        -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
)

# ------------------------------------------------------------
# kernel.bin
# ------------------------------------------------------------
add_custom_command(
        OUTPUT ${BUILD_DIR}/kernel.bin
        COMMAND ${OBJCOPY_EXECUTABLE} -O binary kernel.elf kernel.bin
        DEPENDS kernel.elf
        WORKING_DIRECTORY ${BUILD_DIR}
        COMMENT "Generating kernel.bin"
)

# ------------------------------------------------------------
# Disk image
# ------------------------------------------------------------
add_custom_command(
        OUTPUT ${BUILD_DIR}/disk.img
        COMMAND dd if=/dev/zero of=disk.img bs=512 count=8192
        COMMAND dd if=bootsector.bin of=disk.img conv=notrunc
        COMMAND dd if=loader.bin     of=disk.img bs=512 seek=1 conv=notrunc
        COMMAND dd if=kernel.bin     of=disk.img bs=512 seek=3 conv=notrunc
        DEPENDS
        ${BUILD_DIR}/bootsector.bin
        ${BUILD_DIR}/loader.bin
        ${BUILD_DIR}/kernel.bin
        WORKING_DIRECTORY ${BUILD_DIR}
        COMMENT "Building disk.img"
)

add_custom_target(disk ALL
        DEPENDS ${BUILD_DIR}/disk.img
)

# ------------------------------------------------------------
# Run (QEMU)
# ------------------------------------------------------------
add_custom_target(run
        COMMAND ${QEMU_EXECUTABLE} -no-reboot -no-shutdown -drive format=raw,file=${BUILD_DIR}/disk.img
        DEPENDS ${BUILD_DIR}/disk.img
        WORKING_DIRECTORY ${BUILD_DIR}
        COMMENT "Running PUnix in QEMU"
)
