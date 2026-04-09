set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv64)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_COMPILER riscv64-unknown-elf-gcc)
set(CMAKE_ASM_COMPILER riscv64-unknown-elf-gcc)
set(CMAKE_OBJCOPY riscv64-unknown-elf-objcopy)

set(RCORE_ARCH_FLAGS "-march=rv64gc -mabi=lp64 -mcmodel=medany")

set(CMAKE_C_FLAGS_INIT "${RCORE_ARCH_FLAGS} -ffreestanding -fno-builtin -fno-stack-protector -Wall -Wextra")
set(CMAKE_ASM_FLAGS_INIT "${RCORE_ARCH_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${RCORE_ARCH_FLAGS} -nostdlib -nostartfiles -Wl,--build-id=none")
