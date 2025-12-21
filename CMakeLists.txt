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
set(APPS_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/apps)
set(LIBS_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/libs)
set(DRIVERS_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/drivers)
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
# Kernel core sources
# ------------------------------------------------------------
set(KERNEL_CORE_SOURCES
        ${KERNEL_DIR}/core/kernel.c
        ${KERNEL_DIR}/core/sched.c
        ${KERNEL_DIR}/core/syscall.c
        ${KERNEL_DIR}/core/kutils.c
        ${KERNEL_DIR}/core/elf_loader.c
        ${KERNEL_DIR}/core/task_table.c
        ${KERNEL_DIR}/core/tty.c
        ${KERNEL_DIR}/core/wait.c
        ${KERNEL_DIR}/core/mm.c
        arch/x86/panic.c
)

set(KERNEL_DRIVER_SOURCES
        ${DRIVERS_DIR}/console.c
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
        ${ARCH_SRC_DIR}/gdt.c
        ${ARCH_SRC_DIR}/irq.c
        ${ARCH_SRC_DIR}/keyboard.c
        ${ARCH_SRC_DIR}/syscall.c
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
# Helper: build & embed a user app
# ------------------------------------------------------------
function(add_app name)
    add_executable(${name}.elf
            ${APPS_DIR}/${name}.c
            ${LIBS_DIR}/libc.c
            ${ARCH_SRC_DIR}/libc.c
    )

    target_compile_options(${name}.elf PRIVATE
            -ffreestanding -nostdlib -m32 -O2 -Wall -Wextra
            -ffunction-sections -fdata-sections
            -fPIE
            -fno-asynchronous-unwind-tables
    )

    target_link_options(${name}.elf PRIVATE
            -m32 -nostdlib
            -pie
            -Wl,-e,_start
            -Wl,-u,main
            -Wl,--gc-sections
            -Wl,-z,max-page-size=0x1
            -Wl,-z,common-page-size=0x1
            -Wl,--build-id=none
            -Wl,--strip-all
            -Wl,--no-dynamic-linker
            -nostartfiles
    )

    set_target_properties(${name}.elf PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR}
    )

    add_custom_command(TARGET ${name}.elf POST_BUILD
            COMMAND ${CMAKE_STRIP} --strip-all ${BUILD_DIR}/${name}.elf
            COMMAND ${OBJCOPY_EXECUTABLE} --remove-section=.comment ${BUILD_DIR}/${name}.elf
            COMMAND ${OBJCOPY_EXECUTABLE} --remove-section=.note.gnu.property ${BUILD_DIR}/${name}.elf
            COMMAND ${OBJCOPY_EXECUTABLE} -R .shstrtab -R .strtab ${BUILD_DIR}/${name}.elf
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
# Build user apps
# ------------------------------------------------------------
add_app(init)
add_app(sh)
add_app(ls)
add_app(ps)
add_app(loop)
add_app(spawn_chain)
add_app(kill)
add_app(cat)
add_app(echo)
add_app(swapper)
add_app(print_env)
add_app(tty)
add_app(pwd)

# ------------------------------------------------------------
# Kernel ELF
# ------------------------------------------------------------
add_executable(kernel.elf
        $<TARGET_OBJECTS:kernel_objs>
        ${SCHED_ASM_OBJ}
        ${init_OBJ}
        ${sh_OBJ}
        ${ls_OBJ}
        ${ps_OBJ}
        ${loop_OBJ}
        ${spawn_chain_OBJ}
        ${kill_OBJ}
        ${cat_OBJ}
        ${echo_OBJ}
        ${swapper_OBJ}
        ${print_env_OBJ}
        ${tty_OBJ}
        ${pwd_OBJ}
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
        COMMAND ${QEMU_EXECUTABLE} -drive format=raw,file=${BUILD_DIR}/disk.img
        DEPENDS ${BUILD_DIR}/disk.img
        WORKING_DIRECTORY ${BUILD_DIR}
        COMMENT "Running PUnix in QEMU"
)
