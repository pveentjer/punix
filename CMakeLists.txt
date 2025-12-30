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
        print_env tty pwd date uptime clear time
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
        "/bin/print_env"
        "/bin/tty"
        "/bin/pwd"
        "/bin/date"
        "/bin/uptime"
        "/bin/clear"
        "/bin/time"
)

# ------------------------------------------------------------
# Generate embedded_apps.c
# ------------------------------------------------------------
file(WRITE ${BUILD_DIR}/embedded_apps.c "#include \"kernel/kutils.h\"\n")
file(APPEND ${BUILD_DIR}/embedded_apps.c "#include \"kernel/console.h\"\n")
file(APPEND ${BUILD_DIR}/embedded_apps.c "#include \"kernel/elf_loader.h\"\n\n")
file(APPEND ${BUILD_DIR}/embedded_apps.c "/* Auto-generated - do not edit */\n\n")

foreach(name ${ALL_BINS})
    file(APPEND ${BUILD_DIR}/embedded_apps.c "extern unsigned char _binary_${name}_elf_start[];\n")
    file(APPEND ${BUILD_DIR}/embedded_apps.c "extern unsigned char _binary_${name}_elf_end[];\n")
endforeach()

file(APPEND ${BUILD_DIR}/embedded_apps.c "\nconst struct embedded_app embedded_apps[] = {\n")

list(LENGTH ALL_BINS count)
math(EXPR last "${count} - 1")
foreach(idx RANGE ${last})
    list(GET ALL_BINS ${idx} name)
    list(GET BIN_PATHS ${idx} path)
    file(APPEND ${BUILD_DIR}/embedded_apps.c "    {\"${path}\", _binary_${name}_elf_start, _binary_${name}_elf_end},\n")
endforeach()

file(APPEND ${BUILD_DIR}/embedded_apps.c "};\n\n")
file(APPEND ${BUILD_DIR}/embedded_apps.c "const size_t embedded_app_count = sizeof(embedded_apps) / sizeof(embedded_apps[0]);\n\n")
file(APPEND ${BUILD_DIR}/embedded_apps.c "
const struct embedded_app *find_app(const char *name)
{
    for (size_t i = 0; i < embedded_app_count; i++)
        if (k_strcmp(embedded_apps[i].name, name) == 0)
            return &embedded_apps[i];
    return NULL;
}
")

# ------------------------------------------------------------
# Kernel core sources
# ------------------------------------------------------------
set(KERNEL_CORE_SOURCES
        ${KERNEL_DIR}/core/kernel.c
        ${KERNEL_DIR}/core/sched.c
        ${KERNEL_DIR}/core/syscall.c
        ${KERNEL_DIR}/core/kutils.c
        ${KERNEL_DIR}/core/elf_loader.c
        ${BUILD_DIR}/embedded_apps.c
        ${KERNEL_DIR}/core/task_table.c
        ${KERNEL_DIR}/core/tty.c
        ${KERNEL_DIR}/core/wait.c
        ${KERNEL_DIR}/core/mm.c
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
        ${ARCH_SRC_DIR}/syscall.c
        ${ARCH_SRC_DIR}/clock.c
        ${ARCH_SRC_DIR}/vm.c
        ${ARCH_SRC_DIR}/console.c
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
# Helper: build & embed a binary
# ------------------------------------------------------------
function(add_bin name)
    add_executable(${name}.elf
            ${BIN_DIR}/${name}.c
            ${LIBS_DIR}/libc.c
            ${ARCH_SRC_DIR}/libc.c
    )

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
            -Wl,-e,main
            -Wl,-u,main
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
        COMMAND dd if=/dev/zero of=disk.img bs=512 count=2880
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