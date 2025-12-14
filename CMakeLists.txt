cmake_minimum_required(VERSION 3.20)

project(PUnix C ASM)

# ------------------------------------------------------------
# Tool detection
# ------------------------------------------------------------
find_program(NASM_EXECUTABLE nasm REQUIRED)
find_program(OBJCOPY_EXECUTABLE objcopy REQUIRED)
find_program(QEMU_EXECUTABLE qemu-system-i386 REQUIRED)
find_program(LD_EXECUTABLE ld REQUIRED)

# ------------------------------------------------------------
# Compiler / flags
# ------------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(COMMON_C_FLAGS "-ffreestanding -nostdlib -m32 -O2 -Wall -Wextra")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_C_FLAGS}")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(BOOT_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/boot)
set(KERNEL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/kernel)
set(APPS_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/apps)
set(BUILD_DIR  ${CMAKE_CURRENT_BINARY_DIR})

# ------------------------------------------------------------
# Boot sector and loader (NASM -> .bin)
# ------------------------------------------------------------
add_custom_command(
        OUTPUT ${BUILD_DIR}/bootsector.bin
        COMMAND ${NASM_EXECUTABLE} -f bin ${BOOT_DIR}/arch/x86/bootsector.asm -o bootsector.bin
        DEPENDS ${BOOT_DIR}/arch/x86/bootsector.asm
        WORKING_DIRECTORY ${BUILD_DIR}
        COMMENT "Assembling bootsector.bin"
)

add_custom_command(
        OUTPUT ${BUILD_DIR}/loader.bin
        COMMAND ${NASM_EXECUTABLE} -f bin ${BOOT_DIR}/arch/x86/loader.asm -o loader.bin
        DEPENDS ${BOOT_DIR}/arch/x86/loader.asm
        WORKING_DIRECTORY ${BUILD_DIR}
        COMMENT "Assembling loader.bin"
)

# ------------------------------------------------------------
# Kernel core sources
# ------------------------------------------------------------
set(KERNEL_CORE_SOURCES
        ${KERNEL_DIR}/core/kernel.c
        ${KERNEL_DIR}/core/sched.c
        ${KERNEL_DIR}/core/interrupt.c
        ${KERNEL_DIR}/core/kernel_api.c
        ${KERNEL_DIR}/core/kutils.c
        ${KERNEL_DIR}/core/elf.c
)

set(KERNEL_DRIVER_SOURCES
        ${KERNEL_DIR}/drivers/console.c
        ${KERNEL_DIR}/drivers/keyboard.c
)

set(KERNEL_SOURCES
        ${KERNEL_CORE_SOURCES}
        ${KERNEL_DRIVER_SOURCES}
        kernel/core/gdt.c
        kernel/core/task_table.c
        kernel/core/fs/vfs.c
        kernel/core/files.c
        kernel/core/fs/bin_fs.c
        kernel/core/fs/dev_fs.c
        kernel/core/fs/fs_util.c
        kernel/core/fs/proc_fs.c
        kernel/core/fs/root_fs.c
        kernel/core/tty.c
        kernel/core/wait.c
)

add_library(kernel_objs OBJECT ${KERNEL_SOURCES})

# ------------------------------------------------------------
# sched_x86.asm -> sched_x86.o (NASM, ELF32)
# ------------------------------------------------------------
add_custom_command(
        OUTPUT ${BUILD_DIR}/sched_x86.o
        COMMAND ${NASM_EXECUTABLE} -f elf32
        ${KERNEL_DIR}/arch/x86/sched_x86.asm
        -o sched_x86.o
        DEPENDS ${KERNEL_DIR}/arch/x86/sched_x86.asm
        WORKING_DIRECTORY ${BUILD_DIR}
        COMMENT "Assembling sched_x86.o"
)

# ------------------------------------------------------------
# Helper: build & embed a user app
# ------------------------------------------------------------
function(add_app name)
    add_executable(${name}.elf
            ${APPS_DIR}/${name}.c
            ${APPS_DIR}/libc.c
    )

    target_compile_options(${name}.elf PRIVATE
            -ffreestanding -nostdlib -m32 -O2 -Wall -Wextra
            -fPIE
            -fno-asynchronous-unwind-tables
    )

    target_link_options(${name}.elf PRIVATE
            -m32 -nostdlib
            -pie
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

    # Strip section headers and unnecessary sections after build
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
add_app(test_args)
add_app(spawn_chain)
add_app(kill)
add_app(cat)
add_app(echo)
add_app(swapper)
add_app(print_env)
add_app(tty)
add_app(pwd)

# ------------------------------------------------------------
# Kernel ELF target
# ------------------------------------------------------------
add_executable(kernel.elf
        $<TARGET_OBJECTS:kernel_objs>
        ${BUILD_DIR}/sched_x86.o
        ${init_OBJ}
        ${sh_OBJ}
        ${ls_OBJ}
        ${ps_OBJ}
        ${loop_OBJ}
        ${test_args_OBJ}
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
# kernel.elf -> kernel.bin
# ------------------------------------------------------------
add_custom_command(
        OUTPUT ${BUILD_DIR}/kernel.bin
        COMMAND ${OBJCOPY_EXECUTABLE} -O binary kernel.elf kernel.bin
        DEPENDS kernel.elf
        WORKING_DIRECTORY ${BUILD_DIR}
        COMMENT "Generating kernel.bin"
)

# ------------------------------------------------------------
# Disk image (bootsector + loader + kernel)
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
# Run target (QEMU)
# ------------------------------------------------------------
add_custom_target(run
        COMMAND ${QEMU_EXECUTABLE} -drive format=raw,file=${BUILD_DIR}/disk.img
        DEPENDS ${BUILD_DIR}/disk.img
        WORKING_DIRECTORY ${BUILD_DIR}
        COMMENT "Running Munix in QEMU"
)
