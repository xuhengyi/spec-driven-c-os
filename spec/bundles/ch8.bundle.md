# ch8 bundle

## Current Chapter

### ch8/spec.md
Source: spec/specs/ch8/spec.md

# Capability: ch8

## Purpose

`ch8` capability 表示教程第八章的线程与同步内核：它在信号内核基础上引入线程管理与同步原语相关系统调用，使同一进程内可以拥有多个执行流并通过同步对象协作。

## Requirements

### Requirement: 为进程接入线程管理

`ch8` capability SHALL 在进程管理之上维护线程集合，并允许在同一地址空间内创建和调度多个线程。

#### Scenario: 进程创建新线程

- **WHEN** 用户程序发起线程创建相关系统调用
- **THEN** capability MUST 为该线程建立上下文并纳入现有调度体系

### Requirement: 接入同步原语相关系统调用

`ch8` capability SHALL 为用户程序提供互斥、条件变量和信号量等同步对象相关系统调用。

#### Scenario: 线程等待同步对象

- **WHEN** 某个线程在互斥锁、条件变量或信号量上发起等待
- **THEN** capability MUST 通过内核同步层维护其阻塞与唤醒关系

### Requirement: 在线程级调度中保持进程语义

`ch8` capability SHALL 在引入线程后继续维持进程级资源、地址空间和信号语义的一致性。

#### Scenario: 多线程进程继续运行既有进程能力

- **WHEN** 某个进程拥有多个线程并继续使用文件或信号相关能力
- **THEN** capability MUST 保持这些能力与线程调度的组合行为一致

## Public API

- 无稳定 Rust 库 API
- 对外能力体现为带线程与同步支持的章节内核行为

## Build Configuration

- `build.rs` 将 `linker::SCRIPT` 写入 `OUT_DIR/linker.ld`
- 构建依赖 `APP_ASM` 与 `LOG`

## Dependencies

- `rcore-task-manage` with `proc,thread`
- `sync`
- `signal`
- `signal-impl`
- `syscall` with `kernel`
### ch8/design.md
Source: spec/specs/ch8/design.md

## Context

`ch8` 把线程与同步原语叠加到已经具备进程、文件系统和信号能力的内核上，因此复杂性主要来自子系统之间的组合，而不是单一模块本身。

## Key Decisions

- `spec.md` 只固定“线程创建/调度 + 同步 syscall + 维持进程语义”的对外能力
- 具体线程关系和同步对象实现细节分别留在 `task-manage` 与 `sync` 侧

## Constraints

- 线程和进程共享地址空间与部分资源
- 同步行为依赖调度器与阻塞唤醒路径
- 既有文件系统与信号语义不能因线程引入而被破坏

## Follow-up Split Notes

- chapter spec 不拆分
- 第二轮若参考 tests，应重点补强线程创建失败、同步对象错误路径等场景


## Direct Dependency Specs

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

### sync/spec.md
Source: spec/specs/sync/spec.md

# Capability: sync

## Purpose

`sync` capability 用于作为 `sync-up-cell` 与 `sync-blocking-primitives` 的聚合层，描述章节内核如何通过统一 crate 表面接入单处理器同步原语。

## Requirements

### Requirement: 聚合独占访问单元与阻塞同步原语

`sync` capability SHALL 通过同一 crate 暴露 `sync-up-cell` 与 `sync-blocking-primitives` 两层能力，使单处理器临界区保护和阻塞同步原语可以统一接入章节内核。

#### Scenario: 章节内核通过统一 crate 使用多种同步原语

- **WHEN** 章节内核通过 `sync` crate 同时使用 `UPIntrFreeCell`、mutex、condvar 或 semaphore
- **THEN** capability MUST 同时满足 `sync-up-cell` 与 `sync-blocking-primitives` 所定义的契约

### Requirement: 为章节内核保留统一的同步 crate 表面

`sync` capability SHALL 保留 `UPIntrFreeCell`、`MutexBlocking`、`Condvar` 和 `Semaphore` 等统一导出表面，使上层内核继续通过同一 crate 接入同步能力。

#### Scenario: 上层内核继续通过统一 crate 使用同步对象

- **WHEN** 某个章节内核依赖 `sync` crate 管理临界区和线程阻塞
- **THEN** capability MUST 允许该章节通过统一 crate 表面访问所需同步对象

## Public API

- `UPIntrFreeCell`
- `UPIntrRefMut`
- `Mutex`
- `MutexBlocking`
- `Condvar`
- `Semaphore`

## Build Configuration

- 无 `build.rs`
- 非 RISC-V 目标上的中断开关分支只用于测试支持

## Dependencies

- 聚合 `sync-up-cell`
- 聚合 `sync-blocking-primitives`
- `spin`
- `alloc`

### sync/design.md
Source: spec/specs/sync/design.md

## Context

`sync` 同时包含中断屏蔽单元和几类阻塞同步原语。它们都服务于单处理器教程内核，但内部语义层次并不完全相同。

## Key Decisions

- current truth 现在同时保留聚合层 `sync` 和拆分后的 `sync-up-cell`、`sync-blocking-primitives`
- `sync` 自身收缩为聚合 spec，不再重复子 capability 的细节 Requirement

## Constraints

- 该 crate 假定单处理器环境
- 中断屏蔽与调度器唤醒路径都受目标架构和上层调度器设计约束

## Follow-up Split Notes

- 已完成第一版 split：`sync-up-cell` 与 `sync-blocking-primitives`
- 第二轮已确认 `UPIntrFreeCell` 的独占借用约束以及互斥锁/条件变量/信号量的基础唤醒语义
- `wait_with_mutex` 目前仍是源码中显式标注的简化实现，因此没有在 spec 中把其当前时序写死为 MUST

### signal/spec.md
Source: spec/specs/signal/spec.md

# Capability: signal

## Purpose

`signal` capability 用于定义教程信号子系统对外暴露的统一 trait 和处理结果枚举，并重导出信号编号与动作定义。

## Requirements

### Requirement: 提供统一的信号接口 trait

`signal` capability SHALL 定义一个 `Signal` trait，用于抽象信号状态复制、清理、递送、掩码管理和处理动作管理。

#### Scenario: 章节内核持有抽象信号对象

- **WHEN** 上层代码只依赖信号行为而不依赖具体实现
- **THEN** capability MUST 允许它通过 `Signal` trait 访问这些操作

#### Scenario: fork 后继承掩码与处理动作

- **WHEN** 调用方在某个 `Signal` 对象上执行 `from_fork`
- **THEN** 产生的新对象 MUST 继承原对象的信号掩码和处理动作配置

#### Scenario: 更新掩码时返回旧值

- **WHEN** 调用方通过 `update_mask` 写入新的信号掩码
- **THEN** capability MUST 返回更新前的旧掩码值

### Requirement: 提供信号处理结果表示

`signal` capability SHALL 定义 `SignalResult`，用于表达当前是否需要进入信号处理、冻结或继续执行。

#### Scenario: 陷入处理阶段查询信号结果

- **WHEN** 内核在返回用户态之前检查信号状态
- **THEN** capability MUST 通过 `SignalResult` 传达下一步处理分支

#### Scenario: 结果枚举可表达终止与暂停

- **WHEN** 某次信号处理需要表达继续执行以外的控制流结果
- **THEN** capability MUST 能以 `SignalResult` 表示忽略、已处理、终止或暂停等分支

### Requirement: 重导出共享的信号定义

`signal` capability SHALL 重导出 `SignalAction`、`SignalNo` 和 `MAX_SIG`，使上层模块可以通过一个统一依赖入口获取信号接口与数据定义。

#### Scenario: 使用方只依赖 `signal` crate

- **WHEN** 某个模块同时需要信号 trait 和信号号定义
- **THEN** capability MUST 允许它只依赖 `signal` crate 获取这些类型

## Public API

- `Signal`
- `SignalResult`
- `SignalAction`
- `SignalNo`
- `MAX_SIG`

## Build Configuration

- 无 `build.rs`

## Dependencies

- `signal-defs`

### signal-impl/spec.md
Source: spec/specs/signal-impl/spec.md

# Capability: signal-impl

## Purpose

`signal-impl` capability 用于为教程内核提供 `signal::Signal` trait 的默认实现，负责维护进程的待处理信号、屏蔽集合、处理动作和当前处理状态。

## Requirements

### Requirement: 提供可维护的信号状态对象

`signal-impl` capability SHALL 提供一个 `SignalImpl` 类型，用于统一保存待处理信号集合、屏蔽集合、已注册动作和当前处理状态。

#### Scenario: 进程初始化自己的信号状态

- **WHEN** 章节内核为某个进程创建信号子系统对象
- **THEN** capability MUST 生成一个可后续接收、屏蔽和处理信号的状态实例

#### Scenario: 清空后动作表恢复默认状态

- **WHEN** 调用方对某个 `SignalImpl` 执行 `clear`
- **THEN** capability MUST 丢弃已安装的处理动作并恢复默认动作配置

### Requirement: 实现标准信号接口

`signal-impl` capability SHALL 实现 `signal::Signal` trait，使上层内核能够通过统一接口递送信号、更新掩码、设置动作并推进信号处理流程。

#### Scenario: 内核向进程递送一个信号

- **WHEN** 上层模块调用信号递送接口
- **THEN** capability MUST 更新待处理集合，并在后续查询时反映出新的处理结果

#### Scenario: 默认忽略与默认终止信号分支可区分

- **WHEN** 当前待处理信号分别落在默认忽略和默认终止两类动作上
- **THEN** capability MUST 产生与这两类默认动作一致的 `SignalResult`

#### Scenario: 停止后可被继续信号恢复

- **WHEN** 进程因停止类信号进入暂停状态并随后收到继续执行信号
- **THEN** capability MUST 允许该进程从暂停状态恢复

### Requirement: 支持 fork 继承与处理动作恢复

`signal-impl` capability SHALL 支持从父进程复制必要的信号状态，并在信号处理返回时恢复正常执行状态。

#### Scenario: 进程 fork 后继承信号配置

- **WHEN** 内核从父进程派生一个子进程
- **THEN** capability MUST 复制规范中允许继承的信号配置，并为子进程建立独立的运行时状态

#### Scenario: fork 不继承父进程当前待处理信号

- **WHEN** 父进程在存在待处理信号时派生子进程
- **THEN** 子进程 MUST 继承掩码和动作配置，但 MUST NOT 继承父进程当前待处理的信号集合

#### Scenario: 进入用户 handler 后可通过 `sig_return` 恢复原上下文

- **WHEN** 某个用户态信号处理函数已接管当前上下文并随后调用 `sig_return`
- **THEN** capability MUST 恢复进入 handler 之前保存的上下文并退出“正在处理信号”状态

## Public API

- `SignalImpl`
- `HandlingSignal`

## Build Configuration

- 无 `build.rs`

## Dependencies

- `signal`
- `signal-defs`

### signal-impl/design.md
Source: spec/specs/signal-impl/design.md

## Context

`signal-impl` 的核心不是复杂的外部 API，而是内部信号状态机：待处理信号、屏蔽集合、动作表和“当前正在处理哪个信号”的运行时状态。

## Key Decisions

- `spec.md` 只记录外部行为接口，不把内部 bitset 和状态流转细节全部固化成 Requirement
- 用 `design.md` 承接“为什么这个实现需要状态机”的背景

## Constraints

- 它必须符合 `signal::Signal` trait 约定
- 它既要支持信号递送，也要支持 signal handler 返回后的恢复

## Follow-up Split Notes

- 当前没有必要再拆 capability
- 第二轮已根据 tests 补强 fork 继承、默认动作分支、停止/继续和 `sig_return` 恢复场景

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
