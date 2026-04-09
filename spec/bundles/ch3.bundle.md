# ch3 bundle

## Current Chapter

### ch3/spec.md
Source: spec/specs/ch3/spec.md

# Capability: ch3

## Purpose

`ch3` capability 表示教程第三章的多任务内核：它在批处理基础上引入任务控制块、调度与时钟相关流程，并支持协作或抢占式运行。

## Requirements

### Requirement: 管理多个任务的生命周期

`ch3` capability SHALL 为每个用户应用维护任务控制块，并记录其运行、就绪和结束等状态。

#### Scenario: 内核初始化任务集合

- **WHEN** 内核启动并装载全部应用
- **THEN** capability MUST 为每个应用建立对应任务项并纳入调度集合

### Requirement: 执行调度与任务切换

`ch3` capability SHALL 根据当前章节配置选择协作式或带时钟驱动的抢占式运行方式，并在任务之间切换执行。

#### Scenario: 当前任务主动让出或被抢占

- **WHEN** 任务执行到让出点或收到时钟驱动的切换请求
- **THEN** capability MUST 保存当前上下文并恢复下一个可运行任务

### Requirement: 接入时钟与调度相关系统调用

`ch3` capability SHALL 处理与任务切换、时间读取或让出 CPU 有关的系统调用。

#### Scenario: 用户任务请求让出 CPU

- **WHEN** 用户程序发起与调度相关的系统调用
- **THEN** capability MUST 更新任务状态并触发后续调度决策

## Public API

- 无稳定 Rust 库 API
- 对外能力体现为支持多任务的章节内核行为

## Build Configuration

- `build.rs` 将 `linker::SCRIPT` 写入 `OUT_DIR/linker.ld`
- 构建依赖 `APP_ASM` 与 `LOG`
- feature `coop` 影响调度方式

## Dependencies

- `linker`
- `kernel-context`
- `syscall` with `kernel`
- `sbi-rt`

## Direct Dependency Specs

### linker/spec.md
Source: spec/specs/linker/spec.md

# Capability: linker

## Purpose

`linker` capability 用于为教程内核提供统一的链接脚本模板、启动入口宏和运行时布局符号访问接口，使章节内核能够在 qemu-virt 的 RISC-V 平台上按约定布局启动并枚举内嵌应用元数据。

## Requirements

### Requirement: 提供统一的链接脚本模板

`linker` capability SHALL 以常量形式暴露可写出的链接脚本模板，使依赖它的章节 crate 可以在构建时生成满足教程内核布局要求的链接脚本文件。

#### Scenario: 章节构建脚本写出链接脚本

- **WHEN** 某个章节 crate 在 `build.rs` 中把 `SCRIPT` 写入 `OUT_DIR/linker.ld`
- **THEN** 生成的链接脚本 MUST 定义教程内核所需的段布局和导出符号

#### Scenario: 链接脚本包含核心段与布局符号

- **WHEN** 调用方读取 `SCRIPT` 文本内容
- **THEN** 模板 MUST 包含教程内核所需的 `.text`、`.rodata`、`.data`、`.bss`、`.boot` 等核心段标记以及布局起止符号

### Requirement: 提供启动入口宏

`linker` capability SHALL 提供 `boot0!` 宏，用于声明启动栈和高级语言入口函数，使章节内核可以以统一方式完成最早期启动。

#### Scenario: 章节内核声明启动入口

- **WHEN** 章节 crate 使用 `boot0!(rust_main; stack = ...)`
- **THEN** capability MUST 生成与教程启动流程兼容的入口和栈定义

### Requirement: 提供布局与应用枚举接口

`linker` capability SHALL 提供内核布局和应用元数据访问接口，使章节内核能够在运行时读取段边界并按顺序遍历内嵌应用。

#### Scenario: 批处理章节枚举应用

- **WHEN** 章节内核通过 `AppMeta` 或 `AppIterator` 读取内嵌应用元数据
- **THEN** capability MUST 按链接后布局提供一致的起止位置与遍历顺序

#### Scenario: 初始布局可枚举固定区域

- **WHEN** 调用方遍历 `KernelLayout::INIT`
- **THEN** capability MUST 暴露文本段、只读段、数据段和启动段等既定区域标题及其范围

#### Scenario: 应用被复制到固定槽位

- **WHEN** `AppIterator` 以固定槽位模式装载应用映像
- **THEN** capability MUST 按顺序复制每个应用内容，并将槽位尾部未覆盖部分清零

## Public API

- `SCRIPT`
- `boot0!`
- `KernelLayout`
- `KernelRegion`
- `AppMeta`
- `AppIterator`

## Build Configuration

- 本 crate 自身没有 `build.rs`
- 典型用法是由依赖它的章节 crate 在 `build.rs` 中将 `SCRIPT` 写入 `OUT_DIR`

## Dependencies

- 依赖 `core`
- 不要求额外的运行时服务

### kernel-context/spec.md
Source: spec/specs/kernel-context/spec.md

# Capability: kernel-context

## Purpose

`kernel-context` capability 用于作为 `kernel-context-local` 与 `kernel-context-foreign` 的聚合层，描述章节内核如何通过统一 crate 表面接入本地上下文和 foreign portal 两类能力。

## Requirements

### Requirement: 聚合本地上下文与 foreign 上下文能力

`kernel-context` capability SHALL 通过同一 crate 暴露 `kernel-context-local` 与 `kernel-context-foreign` 两层能力，使章节内核能够同时使用本地执行上下文和跨地址空间中转执行支撑。

#### Scenario: 章节内核同时使用本地和 foreign 能力

- **WHEN** 章节内核需要设置本地执行上下文并在启用 `foreign` feature 后使用中转执行
- **THEN** capability MUST 同时满足 `kernel-context-local` 与 `kernel-context-foreign` 所定义的契约

### Requirement: 为章节内核保留统一的上下文 crate 表面

`kernel-context` capability SHALL 保留 `LocalContext` 以及 `foreign` feature 下 portal 相关类型的统一导出表面，使章节内核不必拆分成多个 crate 依赖。

#### Scenario: 章节内核通过统一 crate 完成地址空间切换准备

- **WHEN** `ch4` 及之后的章节内核通过 `kernel-context` crate 准备用户上下文和中转页布局
- **THEN** capability MUST 允许这些章节继续通过统一 crate 表面访问所需类型

## Public API

- `LocalContext`
- `foreign::MultislotPortal` with `foreign`
- `foreign::PortalCache` with `foreign`
- `foreign::ForeignPortal` with `foreign`
- `foreign::MonoForeignPortal` with `foreign`
- `foreign::ForeignContext` with `foreign`

## Build Configuration

- 无 `build.rs`
- feature `foreign` 打开跨地址空间中转执行支持
- 真实执行路径依赖 `riscv64` 目标

## Dependencies

- 聚合 `kernel-context-local`
- 聚合 `kernel-context-foreign`
- `core`
- `spin` with `foreign`

### kernel-context/design.md
Source: spec/specs/kernel-context/design.md

## Context

`kernel-context` 同时覆盖了普通线程上下文和跨地址空间中转执行两类能力，并直接受 RISC-V CSR、裸函数和中转页布局约束。

## Key Decisions

- current truth 现在同时保留聚合层 `kernel-context` 和拆分后的 `kernel-context-local`、`kernel-context-foreign`
- `kernel-context` 自身收缩为聚合 spec，不再重复子 capability 的细节 Requirement
- 非 `riscv64` 分支仍只服务于测试与 host 编译，不构成主要契约

## Constraints

- 执行路径依赖 `sstatus`、`sepc`、`stvec` 和通用寄存器布局
- foreign portal 依赖可复制的中转代码和公共地址空间映射

## Follow-up Split Notes

- 已完成第一版 split：`kernel-context-local` 与 `kernel-context-foreign`
- 如果后续继续细化，可在 foreign 层中再区分 cache 布局与 execute 路径

### syscall/spec.md
Source: spec/specs/syscall/spec.md

# Capability: syscall

## Purpose

`syscall` capability 用于作为 `syscall-abi`、`syscall-user-api` 和 `syscall-kernel-dispatch` 的聚合层，描述章节内核如何通过统一 crate 表面接入系统调用能力。

## Requirements

### Requirement: 聚合 ABI、用户态封装和内核态分发能力

`syscall` capability SHALL 通过同一 crate 暴露 `syscall-abi`、`syscall-user-api` 和 `syscall-kernel-dispatch` 三层能力，使 ABI、用户态封装和内核态分发保持一致。

#### Scenario: 同一 crate 同时服务用户态和内核态

- **WHEN** 用户程序通过 `user` feature 调用系统调用封装，同时章节内核通过 `kernel` feature 使用分发入口
- **THEN** capability MUST 同时满足 `syscall-abi`、`syscall-user-api` 和 `syscall-kernel-dispatch` 所定义的契约

### Requirement: 为章节和用户程序保留统一的系统调用 crate 表面

`syscall` capability SHALL 保留统一的 crate 表面，使用户程序和章节内核继续通过同一个 crate 访问各自需要的系统调用能力。

#### Scenario: 章节和用户程序继续共享同一 crate 名称

- **WHEN** 现有章节内核或用户程序依赖 `syscall` crate
- **THEN** capability MUST 允许它们在不拆分 crate 依赖名的前提下访问所需接口

## Public API

- `SyscallId`
- `io::*`
- `time::*`
- `user::*` with `user`
- `kernel::handle` with `kernel`
- kernel service traits with `kernel`

## Build Configuration

- `build.rs` 从 `src/syscall.h.in` 生成 `src/syscalls.rs`
- feature `user` 打开用户态封装
- feature `kernel` 打开内核态分发接口

## Dependencies

- 聚合 `syscall-abi`
- 聚合 `syscall-user-api`
- 聚合 `syscall-kernel-dispatch`
- `signal-defs`
- `bitflags`

### syscall/design.md
Source: spec/specs/syscall/design.md

## Context

`syscall` 同时覆盖了编号定义、用户态封装和内核态分发，是一个非常明显的“一个 crate 内有多层能力”的例子。

## Key Decisions

- current truth 现在同时保留聚合层 `syscall` 和拆分后的 `syscall-abi`、`syscall-user-api`、`syscall-kernel-dispatch`
- `syscall` 自身收缩为聚合 spec，不再重复子 capability 的细节 Requirement

## Constraints

- 用户态路径依赖 `riscv64` `ecall`
- 内核态路径依赖章节内核注入具体服务实现
- 编号定义受 `syscall.h.in` 模板驱动

## Follow-up Split Notes

- 已完成第一版 split：`syscall-abi`、`syscall-user-api`、`syscall-kernel-dispatch`
- 第二轮已用 tests 补强 ABI 常量稳定性以及 `user` feature 下的辅助类型与高层包装函数可用性
- 用户态封装的非 `riscv64` host stub 仍只视为测试支持层，不写入主要契约
