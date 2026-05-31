# spec-driven-c-os

`spec-driven-c-os` 是一个面向教学操作系统实验的仓库，它把规格驱动的章节目标、C 语言章节内核实现、运行时资产和最终产物放进同一个代码库，用来验证一条 `spec -> C kernel` 的完整路径。

这个项目做了三件事：

- 把章节目标整理成可直接消费的规格与 bundle
- 产出 `ch2` 到 `ch8` 的 C 语言章节内核源码
- 保留每章最终构建产物，并提供可复现的验证入口

## 项目做了什么

仓库围绕 `rCore` 风格教程内核的章节推进方式展开，但实现目标不是复刻 Rust 版本，而是把章节能力逐步收敛为 C 语言内核实现。

当前仓库已经包含：

- `spec/`：章节与 capability 规格
- `user/`：用户态测例源码与 `cases.toml`
- `trial-workspaces/chX/workspace/c-port/`：每章最终保留的 C 源码
- `runtime-assets/`：构建和验证所需的固定运行时资产
- `deliverables/chapters/chX/`：每章最终导出的 `bin/elf/map`
- `experiments/v2/`：V2 紧凑 COS 章节快照、`spec-v2`、生成 bundle 和共享用户态测例
- `docs/v2-cos-lineage.md`：记录 V2 尝试和本仓库正式 C 版本的关系

## 这个项目怎么做

核心流程是：

1. 用 `spec/specs/*` 和 `spec/bundles/ch*.bundle.md` 描述章节目标
2. 在 `trial-workspaces/chX/workspace/c-port/` 中维护章节级 C 内核实现
3. 通过 `agent.cli verify-chapter chX` 执行章节验证
4. 将最终通过版本导出到 `deliverables/chapters/chX/`

## V2 尝试

`experiments/v2/` 中保留了一套按 `spec-v2` 和章节 bundle 生成的 COS
`ch2` 到 `ch8` QEMU/RISC-V 章节快照。它们是论文一致性实验的 V2
证据，用于说明中性 spec 可以驱动紧凑 C 实现通过共同 base 测例；本仓库的
正式 Phase C 主线仍是 cleaned bundles、CMake 构建、runtime assets、
chapter deliverables 和验证 CLI。

两者的详细关系、测试矩阵和 bundle hash 见
[`docs/v2-cos-lineage.md`](docs/v2-cos-lineage.md)。

验证时会使用仓库内置的：

- `runtime-assets/common/rustsbi-qemu.bin`
- `runtime-assets/fs/fs.img`
- `runtime-assets/user/chX/app.asm`
- `runtime-assets/baselines/chX/case-summary.json`

## 结果

当前 `ch2` 到 `ch8` 已全部通过章节验证。

| Chapter | Passed Cases | Source | Deliverables |
| --- | ---: | --- | --- |
| ch2 | 5 | `trial-workspaces/ch2/workspace/c-port/` | `deliverables/chapters/ch2/` |
| ch3 | 12 | `trial-workspaces/ch3/workspace/c-port/` | `deliverables/chapters/ch3/` |
| ch4 | 12 | `trial-workspaces/ch4/workspace/c-port/` | `deliverables/chapters/ch4/` |
| ch5 | 11 | `trial-workspaces/ch5/workspace/c-port/` | `deliverables/chapters/ch5/` |
| ch6 | 13 | `trial-workspaces/ch6/workspace/c-port/` | `deliverables/chapters/ch6/` |
| ch7 | 17 | `trial-workspaces/ch7/workspace/c-port/` | `deliverables/chapters/ch7/` |
| ch8 | 23 | `trial-workspaces/ch8/workspace/c-port/` | `deliverables/chapters/ch8/` |

最终产物目录中包含：

- `chX.bin`
- `chX.elf`
- `chX.map`
- `manifest.json`

## 快速上手

### 验证某一章

在仓库根目录执行：

```bash
python3 -m agent.cli verify-chapter ch8
```

这条命令会：

1. 读取仓库内置 baseline
2. 对 `trial-workspaces/ch8/workspace/c-port` 执行 CMake 构建
3. 启动 QEMU
4. 匹配章节输出并给出通过/失败结果

### 直接构建

```bash
cmake -S "trial-workspaces/ch8/workspace/c-port" \
  -B "out/ch8" \
  -DCMAKE_TOOLCHAIN_FILE="trial-workspaces/ch8/workspace/c-port/cmake/riscv64-toolchain.cmake" \
  -DRCORE_CHAPTER=ch8 \
  -DAPP_ASM="$PWD/runtime-assets/user/ch8/app.asm" \
  -DRCORE_ARTIFACT_DIR="$PWD/out/ch8/artifacts"

cmake --build "out/ch8" --target rcore_ch8_kernel -j1
```

### 直接启动最终产物

`ch2` 到 `ch5`：

```bash
qemu-system-riscv64 \
  -machine virt \
  -nographic \
  -bios "runtime-assets/common/rustsbi-qemu.bin" \
  -kernel "deliverables/chapters/ch2/ch2.bin" \
  -smp 1 \
  -m 64M \
  -serial mon:stdio
```

`ch6` 到 `ch8` 需要文件系统镜像：

```bash
qemu-system-riscv64 \
  -machine virt \
  -nographic \
  -bios "runtime-assets/common/rustsbi-qemu.bin" \
  -kernel "deliverables/chapters/ch8/ch8.bin" \
  -smp 1 \
  -m 64M \
  -serial mon:stdio \
  -drive "file=runtime-assets/fs/fs.img,if=none,format=raw,id=x0" \
  -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
```

## 仓库结构

```text
phase-c/
├── spec/
├── user/
├── runtime-assets/
├── trial-workspaces/
├── deliverables/
├── agent/
├── tools/
├── LICENSE
└── README.md
```

## License

Repository-specific Phase C code is licensed under the MIT License. See
[LICENSE](./LICENSE).

Archived or third-party inputs may carry their own package metadata. In
particular, check `experiments/v2/**/Cargo.toml` before redistributing the V2
archive under a single license statement.
