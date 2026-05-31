# V2 COS experiment

This directory packages the V2 compact COS attempt.

## Contents

- `spec-v2/`: neutral OS specs.
- `bundles/`: chapter bundles used for generation.
- `cos/ch2` through `cos/ch8`: generated C OS chapter snapshots.
- `user/`: shared user-space apps compiled into chapter app images.
- `tg-rcore-tutorial-console/`, `tg-rcore-tutorial-signal-defs/`, and
  `tg-rcore-tutorial-syscall/`: user-test support crates.
- `agent/`, `prompts/`, and `inputs/`: minimal generation provenance files.

## Run

Each chapter is intended to be run from its own directory:

```bash
cd experiments/v2/cos/ch8
./test.sh base
```

The scripts expect a RISC-V C toolchain, a RISC-V Rust target for user apps,
`rust-objcopy`, QEMU, and `tg-rcore-tutorial-checker` on `PATH`.

## Scope

V2 is a compact consistency artifact. The main Phase C implementation line is
still `trial-workspaces/chX/workspace/c-port/` plus `deliverables/chapters/chX/`.

## License note

The V2 user-test support crates keep their own `Cargo.toml` license metadata.
Check those package manifests before redistributing the V2 archive under a
single repository-level license.
