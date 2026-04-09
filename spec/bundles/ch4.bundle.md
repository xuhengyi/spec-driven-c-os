# ch4 bundle

## Current Chapter

### ch4/spec.md
Source: spec/specs/ch4/spec.md

# Capability: ch4

## Purpose

`ch4` capability 表示教程第四章的进程/地址空间内核：它在前几章基础上引入堆分配、虚拟内存、ELF 装载以及跨地址空间执行所需的中转机制。

## Requirements

### Requirement: 为用户程序建立独立地址空间

`ch4` capability SHALL 为每个用户程序创建独立的地址空间，并把 ELF 段、用户栈、陷入上下文等内容映射到约定位置。

#### Scenario: 内核从 ELF 创建进程

- **WHEN** 内核基于用户程序 ELF 构造新进程
- **THEN** capability MUST 为其建立可执行的用户地址空间并准备初始上下文

### Requirement: 支持跨地址空间执行与返回

`ch4` capability SHALL 借助 `kernel-context` 的 foreign 机制在不同地址空间之间中转执行用户程序，并在陷入时回到内核调度逻辑。

#### Scenario: 进程从内核切换到用户态再陷回

- **WHEN** 调度器选中某个拥有独立地址空间的进程
- **THEN** capability MUST 能够切换到该地址空间执行，并在陷入后恢复内核侧处理流程

### Requirement: 接入内存与进程相关系统调用

`ch4` capability SHALL 提供与地址空间、内存区间和进程执行相关的系统调用实现基础。

#### Scenario: 用户程序查询或修改自身内存相关状态

- **WHEN** 用户程序触发内存或进程相关系统调用
- **THEN** capability MUST 使用当前进程的地址空间和上下文信息完成处理

## Public API

- 无稳定 Rust 库 API
- 对外能力体现为支持进程地址空间的章节内核行为

## Build Configuration

- `build.rs` 将 `linker::SCRIPT` 写入 `OUT_DIR/linker.ld`
- 构建依赖 `APP_ASM` 与 `LOG`
- 依赖 foreign portal 相关能力的目标架构假设

## Dependencies

- `kernel-alloc`
- `kernel-vm`
- `kernel-context` with `foreign`
- `syscall` with `kernel`
- `xmas-elf`
### ch4/design.md
Source: spec/specs/ch4/design.md

## Context

`ch4` 是第一个真正把地址空间、ELF 装载、堆分配和 foreign portal 组合起来的章节。它的外部行为仍然可以用一个章节 capability 表达，但内部架构比前几章复杂得多。

## Key Decisions

- `spec.md` 只描述“建立地址空间、执行进程、处理相关 syscall”的对外能力
- 与页表、中转页和内核常驻映射相关的细节统一放在 `design.md`

## Constraints

- 依赖 `kernel-alloc` 提供堆
- 依赖 `kernel-vm` 提供地址空间
- 依赖 `kernel-context` foreign portal 完成跨地址空间执行

## Follow-up Split Notes

- 后续无需拆 chapter spec，本章的复杂性更适合留在底层 crate 拆分中消化


## Direct Dependency Specs

### kernel-alloc/spec.md
Source: spec/specs/kernel-alloc/spec.md

# Capability: kernel-alloc

## Purpose

`kernel-alloc` capability 用于为教程内核提供一个基于 buddy allocator 的全局堆分配器，并允许在启动后继续向堆中转移新的可用内存区域。

## Requirements

### Requirement: 提供显式初始化入口

`kernel-alloc` capability SHALL 提供 `init` 接口，以便章节内核在已知堆基地址的前提下初始化全局堆分配器。

#### Scenario: 章节内核初始化堆

- **WHEN** 启动代码在确定堆起点后调用 `init`
- **THEN** capability MUST 建立可用于后续动态分配的初始空闲区域

### Requirement: 支持追加可用内存区域

`kernel-alloc` capability SHALL 提供 `transfer` 接口，使调用方能够在初始化之后把额外的连续内存区域加入堆管理。

#### Scenario: 内核把剩余物理页转入堆

- **WHEN** 内核拿到一段新的可分配内存区域并调用 `transfer`
- **THEN** capability MUST 把该区域纳入 buddy allocator 的可分配范围

### Requirement: 在正常构建中作为全局分配器工作

`kernel-alloc` capability SHALL 在非测试构建中将其 buddy allocator 暴露为全局分配器，以支持 `alloc` 生态的动态内存分配。

#### Scenario: 内核代码申请动态内存

- **WHEN** 教程内核在正常构建中触发全局分配
- **THEN** capability MUST 从已初始化并已转入的堆区域中分配内存或报告分配失败

#### Scenario: 多次分配返回互不重叠的有效区间

- **WHEN** 调用方在已初始化的堆上连续申请多个仍处于存活状态的对象
- **THEN** capability MUST 返回可同时使用且互不重叠的内存区间

## Public API

- `init`
- `transfer`

## Build Configuration

- 无 `build.rs`
- 正常构建是 `no_std`
- 测试构建可切换到宿主分配器，但这不属于教程内核运行时契约

## Dependencies

- `alloc`
- `customizable-buddy`

### kernel-alloc/design.md
Source: spec/specs/kernel-alloc/design.md

## Context

`kernel-alloc` 的对外接口极小，但实现上依赖全局静态 buddy allocator 和全局分配器接入。它的主要复杂度来自 unsafe 与堆不变量，而不是 API 面。

## Key Decisions

- `spec.md` 只固定 `init`、`transfer` 和“非测试构建作为全局分配器”的契约
- 测试专用的宿主分配器切换被明确视为实验支持层，不进入主要内核契约

## Constraints

- 该分配器不以内建并发安全为目标
- 调用方必须保证传入区域有效且不重叠
- 初始化和转移的顺序约束由内核启动代码负责

## Follow-up Split Notes

- 当前没有明显的多 capability 拆分需求
- 第二轮若引入 tests，可补强“初始化后追加区域”的场景说明

### kernel-vm/spec.md
Source: spec/specs/kernel-vm/spec.md

# Capability: kernel-vm

## Purpose

`kernel-vm` capability 用于为教程内核提供基于 Sv39 页表的地址空间抽象，并允许上层内核通过 `PageManager` 接口接入具体页分配与页引用计数策略。

## Requirements

### Requirement: 提供地址空间抽象

`kernel-vm` capability SHALL 提供 `AddressSpace` 类型，用于表示可映射、可查询、可复制的内核或用户地址空间。

#### Scenario: 章节内核创建用户地址空间

- **WHEN** 章节内核需要为某个进程创建地址空间
- **THEN** capability MUST 允许它基于给定的 `PageManager` 构造并维护该地址空间

### Requirement: 提供映射与查询能力

`kernel-vm` capability SHALL 支持映射外部页、分配新页、查询虚拟地址翻译结果和访问根页表标识。

#### Scenario: 进程加载 ELF 段

- **WHEN** 内核把某段用户程序内容映射进地址空间
- **THEN** capability MUST 提供相应的映射接口并允许后续按虚拟地址查询翻译结果

#### Scenario: 映射权限影响后续翻译结果

- **WHEN** 某个地址空间以特定权限映射一段虚拟页范围
- **THEN** capability MUST 仅在请求权限与该映射兼容时返回翻译结果

#### Scenario: 外部映射可共享原页内容

- **WHEN** 调用方通过 `map_extern` 将已有物理页映射到另一地址空间
- **THEN** capability MUST 让两个地址空间观察到同一底层页内容

### Requirement: 允许通过 `PageManager` 注入页管理策略

`kernel-vm` capability SHALL 通过 `PageManager` trait 把物理页的分配、回收和元数据管理职责交给使用方。

#### Scenario: 地址空间释放根页表

- **WHEN** 某个地址空间结束生命周期
- **THEN** capability MUST 通过 `PageManager` 接口回收其根页表和相关页资源

#### Scenario: 克隆地址空间获得独立副本

- **WHEN** 调用方将一个地址空间内容克隆到另一个地址空间
- **THEN** capability MUST 为目标地址空间建立独立页副本，使后续对源地址空间的写入不会回写到克隆结果

## Public API

- `AddressSpace`
- `PageManager`
- `page_table` re-exports

## Build Configuration

- 无 `build.rs`
- 默认围绕 Sv39 页表实现

## Dependencies

- `page-table`
- `alloc`

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
