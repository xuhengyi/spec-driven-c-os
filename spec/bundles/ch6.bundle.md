# ch6 bundle

## Current Chapter

### ch6/spec.md
Source: spec/specs/ch6/spec.md

# Capability: ch6

## Purpose

`ch6` capability 表示教程第六章的文件系统内核：它在进程管理内核基础上接入块设备和文件系统，使内核能够从文件系统加载应用并向进程提供文件读写相关系统调用。

## Requirements

### Requirement: 接入块设备并初始化文件系统

`ch6` capability SHALL 初始化块设备访问路径，并在其上打开或创建内核可用的文件系统实例。

#### Scenario: 内核启动时准备文件系统

- **WHEN** 章节内核完成早期内存和地址空间初始化
- **THEN** capability MUST 建立块设备到文件系统管理器的接入路径

### Requirement: 从文件系统加载用户程序

`ch6` capability SHALL 能够通过文件系统定位用户程序映像，并把其内容装载到新进程地址空间。

#### Scenario: init 进程从文件系统载入

- **WHEN** 内核需要启动初始用户程序
- **THEN** capability MUST 能够从文件系统读取目标文件并据此创建进程

### Requirement: 向进程暴露文件相关系统调用

`ch6` capability SHALL 为进程提供文件打开、读写和文件描述符管理所需的系统调用实现基础。

#### Scenario: 用户程序读取文件

- **WHEN** 用户程序发起文件读写相关系统调用
- **THEN** capability MUST 通过文件系统与进程文件描述符状态完成相应操作

## Public API

- 无稳定 Rust 库 API
- 对外能力体现为带文件系统支持的章节内核行为

## Build Configuration

- `build.rs` 将 `linker::SCRIPT` 写入 `OUT_DIR/linker.ld`
- 构建依赖 `APP_ASM` 与 `LOG`
- 运行时依赖块设备与磁盘镜像

## Dependencies

- `easy-fs`
- `kernel-vm`
- `kernel-context` with `foreign`
- `syscall` with `kernel`
- `virtio-drivers`
### ch6/design.md
Source: spec/specs/ch6/design.md

## Context

`ch6` 是第一个把块设备、文件系统和进程系统调用整合进章节内核的版本，因此其复杂度来自子系统接线而非新的单一算法。

## Key Decisions

- `spec.md` 聚焦“文件系统接入、程序装载、文件 syscall”三类外部行为
- 具体的 MMIO 映射、块缓存细节和 FS 内部结构留给底层 crate 的 spec 与 design

## Constraints

- 运行时依赖 virtio block 设备
- 文件系统能力由 `easy-fs` 提供
- 进程与地址空间能力延续自 `ch5`

## Follow-up Split Notes

- chapter spec 不拆分
- 第二轮若引入 tests，可补强文件描述符初始化和错误路径场景


## Direct Dependency Specs

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
