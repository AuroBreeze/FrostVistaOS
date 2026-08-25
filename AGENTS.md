# Repository Guidelines

## 项目结构

FrostVistaOS 是一个面向 RISC-V 64、并处于 LoongArch 早期 bring-up 阶段的小型裸机内核。

- `arch/`：架构相关的启动、陷阱、驱动、分页、链接脚本和构建规则。
- `kernel/core/`：进程、调度、系统调用、exec、文件、管道和信号。
- `kernel/fs/`：VFS，以及 EasyFS、EXT4、tmpfs 和 devtmpfs 实现。
- `kernel/mm/`：页分配器、kmalloc 和 Slab 分配器。
- `include/`：共享内核头文件；`user/`：用户态运行库和应用程序。
- `test/`：用户态测试入口；`mk/`、`mkfs/` 和 `scripts/`：构建片段、文件系统镜像工具和测试运行器。

## 构建、测试和开发命令

默认目标是使用 EasyFS 的 RISC-V 构建：

```bash
make qemu ROOTFS=easyfs FS_LIST="devtmpfs tmpfs" TEST=fvsh
```

常用变量包括 `ARCH=riscv|loongarch`、`ROOTFS=easyfs|ext4`、`BOOT=bare|opensbi`、`TEST=<name>` 和 `BUILD=debug|release`。

```bash
python3 scripts/run_tests.py --list
python3 scripts/run_tests.py -t fvsh_script -T 30
python3 scripts/run_tests.py --check logs/
```

安装对应交叉工具链和 QEMU 后，可使用 `make ARCH=loongarch compdb` 和 `make ARCH=loongarch check-direct-boot` 验证架构构建和直接启动镜像。

## 代码风格

遵循 `.clang-format` 和 `.clang-tidy` 配置。使用 `make format` 格式化代码，使用 `make tidy-file FILE=<path>` 执行单文件检查。架构代码放在 `arch/<arch>/`，共享接口放在 `include/`。C 函数和变量使用 snake_case，宏使用大写命名，文件名使用清晰的小写名称。

## 测试规范

回归测试放在 `test/test_<name>.c`。每个测试应聚焦一个内核路径，并通过 `scripts/run_tests.py` 运行。对于错误路径，应同时验证预期的返回值和内核诊断输出。

## Git 与提交操作

Git、WIP checkpoint、跨设备开发、分支同步、历史整理、提交格式和 Pull Request 的具体要求统一记录在 `docs/development/` 中：

- [`git-development-workflow.md`](docs/development/git-development-workflow.md)：日常开发、特性分支、跨设备同步、WIP 整理和合入 `dev`。
- [`git-commit-workflow.md`](docs/development/git-commit-workflow.md)：Agent 检查改动、规划提交、按完整文件提交和提交后验证的具体要求。

执行 Git 操作前应先阅读与当前任务对应的文档，并遵循其中的分支保护、用户确认和验证要求。
