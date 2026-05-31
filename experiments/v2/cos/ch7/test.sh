#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$script_dir"

case "${1:-}" in
  base)
    export PATH="/usr/local/riscv64-unknown-elf-gcc/bin:${PATH}"
    QEMU="${QEMU:-qemu-system-riscv64}"
    KERNEL="build/cos-ch7.elf"
    make -s all
    "${QEMU}" -machine virt -m 512M -nographic -bios default -kernel "${KERNEL}" \
      | tg-rcore-tutorial-checker --ch 7
    ;;
  exercise)
    echo "exercise mode is excluded from full L1 for the frozen tg-rcore baseline" >&2
    exit 2
    ;;
  *)
    echo "usage: ./test.sh base" >&2
    exit 2
    ;;
esac
