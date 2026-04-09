# ch7 bundle

## Current Chapter

### ch7/spec.md
Source: spec/specs/ch7/spec.md

# Capability: ch7

## Purpose

`ch7` capability 表示教程第七章的信号内核：它在文件系统内核基础上接入信号子系统，使进程能够接收、屏蔽和处理信号。

## Requirements

### Requirement: 为进程接入信号状态

`ch7` capability SHALL 为进程对象接入独立的信号状态，以便在运行、陷入和返回用户态时管理待处理信号。

#### Scenario: 新进程拥有自己的信号子系统

- **WHEN** 内核创建一个新进程
- **THEN** capability MUST 为其准备可独立更新的信号状态对象

### Requirement: 接入信号相关系统调用

`ch7` capability SHALL 提供信号注册、屏蔽和递送相关的系统调用实现基础。

#### Scenario: 用户程序注册信号处理函数

- **WHEN** 用户程序发起信号动作设置或屏蔽更新的系统调用
- **THEN** capability MUST 更新对应进程的信号状态

### Requirement: 在用户态返回前推进信号处理

`ch7` capability SHALL 在适当的内核返回点检查当前进程的信号状态，并在需要时切换到信号处理路径或暂停执行。

#### Scenario: 进程存在待处理信号

- **WHEN** 进程即将从内核返回用户态且存在可处理信号
- **THEN** capability MUST 根据当前屏蔽和动作配置决定后续控制流

## Public API

- 无稳定 Rust 库 API
- 对外能力体现为带信号支持的章节内核行为

## Build Configuration

- `build.rs` 将 `linker::SCRIPT` 写入 `OUT_DIR/linker.ld`
- 构建依赖 `APP_ASM` 与 `LOG`

## Dependencies

- `signal`
- `signal-impl`
- `easy-fs`
- `syscall` with `kernel`
### ch7/design.md
Source: spec/specs/ch7/design.md

## Context

`ch7` 的新增复杂性主要不在文件系统或进程系统，而在“信号什么时候被检查、什么时候改变控制流”。

## Key Decisions

- `spec.md` 只固定信号子系统被集成后的外部能力
- 具体的信号状态机和 handler 返回细节保留在底层 `signal-impl` 侧

## Constraints

- 进程必须在陷入处理和返回用户态之间维护一致的信号状态
- 信号行为叠加在 `ch6` 已有的进程与文件系统能力之上

## Follow-up Split Notes

- chapter spec 不拆分
- 第二轮若参考 tests，应重点确认“何时检查信号”这一时机语义


## Direct Dependency Specs

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

### easy-fs/spec.md
Source: spec/specs/easy-fs/spec.md

# Capability: easy-fs

## Purpose

`easy-fs` capability 用于作为 `easy-fs-storage` 与 `easy-fs-vfs` 的聚合层，描述章节内核如何通过统一 crate 表面接入简化文件系统。

## Requirements

### Requirement: 聚合存储层与 VFS 层能力

`easy-fs` capability SHALL 通过同一 crate 暴露 `easy-fs-storage` 与 `easy-fs-vfs` 两层能力，使章节内核能够同时访问文件系统存储层和 inode/VFS 层接口。

#### Scenario: 章节内核通过统一 crate 接入文件系统

- **WHEN** 章节内核通过 `easy-fs` crate 初始化文件系统并随后访问 inode、目录或文件句柄
- **THEN** capability MUST 同时满足 `easy-fs-storage` 和 `easy-fs-vfs` 所定义的契约

### Requirement: 为章节内核保留统一的文件系统入口表面

`easy-fs` capability SHALL 保留 `EasyFileSystem`、`Inode`、`File`、`OpenFlags` 和 `FSManager` 这一统一入口表面，避免章节内核必须直接依赖多个底层 capability 名称。

#### Scenario: 章节内核以统一入口完成装载和文件访问

- **WHEN** `ch6` 及之后的章节内核通过 `easy-fs` crate 装载用户程序并处理文件相关 syscall
- **THEN** capability MUST 允许这些章节只通过统一 crate 表面访问文件系统能力

## Public API

- `BLOCK_SZ`
- `BlockDevice`
- `EasyFileSystem`
- `Inode`
- `DiskInodeType`
- `OpenFlags`
- `File`
- `FSManager`

## Build Configuration

- 无 `build.rs`
- 依赖使用方提供块设备实现

## Dependencies

- 聚合 `easy-fs-storage`
- 聚合 `easy-fs-vfs`
- `alloc`
- `spin`
- `lazy_static`
- `lru`
- `bitflags`

### easy-fs/design.md
Source: spec/specs/easy-fs/design.md

## Context

`easy-fs` 的源码内部已经天然分成块设备、块缓存、磁盘布局、位图分配、文件系统管理器和 VFS/inode 层。第一轮为了完成全量基线，仍然先保留一个 crate-level spec。

## Key Decisions

- current truth 现在同时保留聚合层 `easy-fs` 和拆分后的 `easy-fs-storage`、`easy-fs-vfs`
- `easy-fs` 自身收缩为聚合 spec，不再重复子 capability 的细节 Requirement
- 把后续更细的拆分留在 storage/VFS 两层之下继续讨论

## Constraints

- 所有数据结构都建立在固定块大小与磁盘布局不变量之上
- 上层章节内核通过 `FSManager` 和文件对象接入它，而不是直接操作所有内部模块

## Follow-up Split Notes

- 已完成第一版 split：`easy-fs-storage` 与 `easy-fs-vfs`
- 后续若要继续细拆，可在 storage 层内再区分 block/cache/layout，在 VFS 层内再区分 inode 与文件句柄

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
