#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:?build dir required}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
user_root="${TG_USER_DIR:-${repo_root}/user}"
target="riscv64gc-unknown-none-elf"
app_base=2248146944
app_step=2097152
apps=(
  00hello_world
  01store_fault
  02power
  03priv_inst
  04priv_csr
  05write_a
  06write_b
  07write_c
  08power_3
  09power_5
  10power_7
  12forktest
  13forktree
  14forktest2
  15matrix
  fork_exit
  forktest_simple
  sbrk
  filetest_simple
  cat_filea
  sig_simple
  sig_simple2
  sig_ctrlc
  sig_tests
  pipetest
  pipe_large_test
  ch7b_usertest
  user_shell
  initproc
)

mkdir -p "${build_dir}/apps"
asm="${build_dir}/apps.S"
user_target_dir="${build_dir}/user-target"
app_target="${user_target_dir}/${target}/debug"

bins=()
for i in "${!apps[@]}"; do
  name="${apps[$i]}"
  base="${app_base}"
  env -u CARGO_ENCODED_RUSTFLAGS \
    RUSTFLAGS="-Aunsafe_op_in_unsafe_fn" \
    CARGO_TARGET_DIR="${user_target_dir}" \
    BASE_ADDRESS="${base}" \
    CHAPTER="-7" \
    cargo build --manifest-path "${user_root}/Cargo.toml" --bin "${name}" --target "${target}" >/dev/null
  bin="${build_dir}/apps/${name}.bin"
  rust-objcopy "${app_target}/${name}" --strip-all -O binary "${bin}"
  bins+=("${bin}")
done

{
  printf '.global apps\n'
  printf '.section .data\n'
  printf '.align 3\n'
  printf 'apps:\n'
  printf '    .quad 0x%x\n' "${app_base}"
  printf '    .quad 0x%x\n' "${app_step}"
  printf '    .quad %d\n' "${#bins[@]}"
  for i in "${!bins[@]}"; do
    printf '    .quad app_%d_start\n' "${i}"
  done
  printf '    .quad app_%d_end\n' "$((${#bins[@]} - 1))"
  for i in "${!bins[@]}"; do
    printf 'app_%d_start:\n' "${i}"
    printf '    .incbin "%s"\n' "${bins[$i]}"
    printf 'app_%d_end:\n' "${i}"
  done
} > "${asm}"
