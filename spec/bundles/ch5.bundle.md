# ch5 bundle

## Current Chapter

### ch5/spec.md
Source: spec/specs/ch5/spec.md

# Capability: ch5

## Purpose

`ch5` capability 表示教程第五章的进程管理内核：它在地址空间内核基础上引入更明确的进程管理结构，并支持 `fork`、`exec`、`wait` 等进程相关流程。

## Requirements

### Requirement: 通过进程管理器维护进程集合

`ch5` capability SHALL 使用进程管理结构维护所有活跃进程，并把调度、父子关系和地址空间生命周期关联起来。

#### Scenario: 内核创建并登记一个新进程

- **WHEN** 系统需要创建初始进程或派生新进程
- **THEN** capability MUST 把该进程纳入管理器，并维持其与父进程的关联关系

### Requirement: 支持进程派生与替换执行映像

`ch5` capability SHALL 支持从现有进程派生新进程，并允许进程替换自己的执行映像。

#### Scenario: 进程执行 `fork` 或 `exec`

- **WHEN** 用户程序发起派生或替换执行映像的系统调用
- **THEN** capability MUST 更新目标进程的上下文、地址空间和管理关系

### Requirement: 支持基本的进程退出与等待路径

`ch5` capability SHALL 提供进程退出和父进程等待子进程结束的基础路径。

#### Scenario: 父进程等待子进程结束

- **WHEN** 父进程调用等待相关系统调用
- **THEN** capability MUST 在子进程结束后返回与之对应的结果信息

## Public API

- 无稳定 Rust 库 API
- 对外能力体现为支持进程管理的章节内核行为

## Build Configuration

- `build.rs` 将 `linker::SCRIPT` 写入 `OUT_DIR/linker.ld`
- 构建依赖 `APP_ASM` 与 `LOG`

## Dependencies

- `kernel-alloc`
- `kernel-vm`
- `kernel-context` with `foreign`
- `rcore-task-manage` with `proc`
- `syscall` with `kernel`
### ch5/design.md
Source: spec/specs/ch5/design.md

## Context

`ch5` 通过 `rcore-task-manage` 的 `proc` 能力把前一章的“能运行进程”提升为“能维护进程集合和关系”。

## Key Decisions

- `spec.md` 聚焦进程生命周期行为，不重复底层地址空间与上下文切换的细节
- 把父子关系维护和等待路径的实现复杂性放在 `design.md` 说明

## Constraints

- 进程关系管理依赖 `rcore-task-manage`
- 地址空间与执行流基础仍继承 `ch4`

## Follow-up Split Notes

- chapter spec 不拆分
- 第二轮若参考 tests，应重点补强 `fork` / `wait` 的边界场景


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

### task-manage/spec.md
Source: spec/specs/task-manage/spec.md

# Capability: task-manage

## Purpose

`task-manage` capability 用于作为 `task-manage-core` 与 `task-manage-relations` 的聚合层，描述章节内核如何通过统一 crate 表面接入任务标识、调度抽象和关系管理能力。

## Requirements

### Requirement: 聚合核心任务抽象与关系管理能力

`task-manage` capability SHALL 通过同一 crate 暴露 `task-manage-core` 与 `task-manage-relations` 两层能力，使上层内核既能获取核心任务抽象，也能在启用 feature 时使用进程/线程关系管理接口。

#### Scenario: 章节内核通过统一 crate 使用任务核心能力和关系能力

- **WHEN** 某个章节内核既需要 `Manage` / `Schedule` 这样的核心任务抽象，又需要 `proc` 或 `thread` feature 下的关系管理结构
- **THEN** capability MUST 同时满足 `task-manage-core` 与 `task-manage-relations` 所定义的契约

### Requirement: 为章节内核保留统一的任务管理 crate 表面

`task-manage` capability SHALL 保留核心任务接口和 feature 化关系接口的统一导出表面，使章节内核继续通过同一 crate 接入任务管理能力。

#### Scenario: 上层内核继续通过统一 crate 使用任务管理接口

- **WHEN** 某个章节内核依赖 `task-manage` crate 管理任务和关系结构
- **THEN** capability MUST 允许该章节继续通过统一 crate 表面访问所需接口

## Public API

- `Manage`
- `Schedule`
- `ProcId`
- `ThreadId`
- `CoroId`
- `PManager` with `proc`
- `ProcRel` with `proc`
- `ProcThreadRel` with `thread`
- `PThreadManager` with `thread`

## Build Configuration

- 无 `build.rs`
- feature `proc` 与 `thread` 决定可用的高层关系管理接口

## Dependencies

- 聚合 `task-manage-core`
- 聚合 `task-manage-relations`
- `alloc`
- `hashbrown`

### task-manage/design.md
Source: spec/specs/task-manage/design.md

## Context

`task-manage` 同时承载了 ID、集合管理、调度抽象，以及 `proc` / `thread` 两层可选关系管理。这让它在第一轮很适合先作为一个 crate-level capability 处理，但也天然存在后续拆分空间。

## Key Decisions

- current truth 现在同时保留聚合层 `task-manage` 和拆分后的 `task-manage-core`、`task-manage-relations`
- `task-manage` 自身收缩为聚合 spec，不再重复子 capability 的细节 Requirement
- 更细的进程关系与线程关系层次仍保留在后续 split 讨论中

## Constraints

- 该 crate 的核心复杂度来自泛型与 feature 组合，而不是架构相关的裸机约束
- 章节内核对它的使用方式随章节推进逐步增强，因此第一轮不宜过度细拆

## Follow-up Split Notes

- 已完成第一版 split：`task-manage-core` 与 `task-manage-relations`
- 后续可继续拆出 `task-id`、`task-schedule`、`proc-rel`、`thread-rel`
- 第二轮 tests 主要确认了 ID 类型的可比较/可哈希性质，以及线程关系层存在“等待退出结果”这一稳定能力
- `Schedule` trait 本身不把 FIFO 顺序写死为 MUST，测试中的 FIFO 只是示例实现行为

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
