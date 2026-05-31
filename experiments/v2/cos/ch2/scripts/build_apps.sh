#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:?build dir required}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
user_root="${TG_USER_DIR:-${repo_root}/user}"
target="riscv64gc-unknown-none-elf"
app_base=2151677952
app_step=0
apps=(
  00hello_world
  01store_fault
  02power
  03priv_inst
  04priv_csr
  08power_3
  09power_5
  10power_7
)

mkdir -p "${build_dir}/apps"
asm="${build_dir}/apps.S"
app_target="${user_root}/target/${target}/debug"

bins=()
for i in "${!apps[@]}"; do
  name="${apps[$i]}"
  base=$((app_base + i * app_step))
  env -u CARGO_ENCODED_RUSTFLAGS \
    RUSTFLAGS="-Aunsafe_op_in_unsafe_fn" \
    BASE_ADDRESS="${base}" \
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
