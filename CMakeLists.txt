cmake_minimum_required(VERSION 3.13)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(RucixOS C ASM_NASM)

set(CMAKE_C_FLAGS "--target=x86_64-pc-none-elf -m64 -ffreestanding -O3 -g -Wall -Wextra -fno-exceptions -fno-rtti -mcmodel=large -mno-red-zone -mno-sse -mno-mmx -mno-avx -I${CMAKE_SOURCE_DIR}/kernel/include")
set(CMAKE_ASM_NASM_FLAGS "-felf64")

set(KERNEL_SOURCES
    boot/trampoline.asm
    boot/multiboot_header.asm
    boot/long_mode_init.asm
    boot/long_mode_start.asm
    kernel/arch/x86_64/gdt.c
    kernel/arch/x86_64/idt.c
    kernel/arch/x86_64/interrupts.asm
    kernel/arch/x86_64/vmx_handler.asm
    kernel/arch/x86_64/switch.asm
    kernel/arch/x86_64/usermode.asm
    kernel/console.c
    kernel/lib/string.c
    kernel/lib/radix-tree.c
    kernel/drivers/pic.c
    kernel/drivers/pit.c
    kernel/drivers/core/driver.c
    kernel/drivers/core/pm.c
    kernel/drivers/core/dma.c
    kernel/drivers/core/irq.c
    kernel/drivers/bus/pci.c
    kernel/drivers/char/chardev.c
    kernel/drivers/char/keyboard.c
    kernel/drivers/char/serial.c
    kernel/drivers/input/input.c
    kernel/drivers/input/mouse.c
    kernel/drivers/input/tablet.c
    kernel/drivers/gpu/drm/drm_core.c
    kernel/drivers/gpu/bga/bga.c
    kernel/drivers/sound/sound_core.c
    kernel/drivers/sound/ac97.c
    kernel/drivers/block/blockdev.c
    kernel/drivers/block/ahci.c
    kernel/drivers/block/nvme.c
    kernel/drivers/usb/usb_core.c
    kernel/drivers/usb/xhci.c
    kernel/drivers/block/elevator.c
    kernel/drivers/block/ramdisk.c
    kernel/memory/pmm.c
    kernel/memory/vmm.c
    kernel/memory/heap.c
    kernel/memory/swap.c
    kernel/virt/vmx.c
    kernel/mm/page_cache.c
    kernel/process/process.c
    kernel/process/sched_fair.c
    kernel/process/sched_mlfq.c
    kernel/process/namespace.c
    kernel/process/cgroup.c
    kernel/security/seccomp.c
    kernel/sync/spinlock.c
    kernel/sync/waitqueue.c
    kernel/sync/rtmutex.c
    kernel/time/hrtimer.c
    kernel/ipc/pipe.c
    kernel/ipc/signal.c
    kernel/ipc/msg.c
    kernel/ipc/shm.c
    kernel/fs/vfs.c
    kernel/fs/ramfs.c
    kernel/fs/fat32.c
    kernel/fs/buffer.c
    kernel/fs/filemap.c
    kernel/process/exec.c
    kernel/syscall/syscall.c
    kernel/net/core/skbuff.c
    kernel/net/core/dev.c
    kernel/net/core/net_namespace.c
    kernel/net/core/netfilter.c
    kernel/net/wireless/core.c
    kernel/net/drivers/loopback.c
    kernel/net/drivers/e1000.c
    kernel/net/drivers/ath5k.c
    kernel/net/ethernet/eth.c
    kernel/net/ipv4/arp.c
    kernel/net/ipv4/ip.c
    kernel/net/ipv4/icmp.c
    kernel/net/ipv4/udp.c
    kernel/net/ipv4/dhcp.c
    kernel/net/ipv4/tcp.c
    kernel/net/socket.c
    kernel/module.c
    kernel/kernel.c
)

add_executable(kernel.elf ${KERNEL_SOURCES})
set_target_properties(kernel.elf PROPERTIES SUFFIX "")
set(LLVM_BIN "C:/Users/CosmerDev/AppData/Local/Android/Sdk/ndk/28.2.13676358/toolchains/llvm/prebuilt/windows-x86_64/bin")
set(CMAKE_C_LINK_EXECUTABLE "${LLVM_BIN}/ld.lld.exe -n -T ${CMAKE_SOURCE_DIR}/linker.ld -m elf_x86_64 -z max-page-size=0x1000 -o <TARGET> <OBJECTS>")

add_custom_command(
    TARGET kernel.elf POST_BUILD
    COMMAND ${LLVM_BIN}/llvm-objcopy --input-target=elf64-x86-64 --output-target=elf32-i386 kernel.elf kernel32.elf
    COMMAND ${LLVM_BIN}/llvm-objcopy -O binary kernel.elf kernel.bin
    COMMENT "Converting kernel to ELF32 and Binary for QEMU"
)

add_custom_target(kernel ALL DEPENDS kernel.elf)
