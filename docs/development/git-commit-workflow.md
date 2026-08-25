# Git Commit Workflow for Agents

本文档记录 Agent 在 FrostVistaOS 仓库中的 Git 提交流程。

## 1. 检查当前改动

提交前先确认工作区状态、改动范围和仓库现有提交风格：

```powershell
git status --short
git diff --stat
git diff --name-only
git log -5 --format=fuller --stat
git diff --check
```

需要时阅读完整 diff，确认改动之间的功能关系，并识别 trailing whitespace、换行或格式问题。

## 2. 按整文件划分提交

- 按功能责任划分提交层次
- 每个提交只包含一个清晰的功能或修复目标
- 只提交完整文件，不拆分 chunk
- 不使用交互式局部暂存
- 同一个文件完整归属于一个提交
- 不将用户未确认的改动、临时文件或无关文件带入提交
- 不使用 reset、checkout 等方式覆盖用户现有改动

常见提交边界包括：

- 构建系统、工具链和开发环境配置
- 单个硬件驱动或平台能力
- 启动流程、异常处理或核心逻辑

## 3. 先提供提交方案供检阅

用户确认前，不执行 `git add` 或 `git commit`。先为每一层提供：

- Conventional Commit subject
- 涉及的完整文件列表
- 英文 commit message
- 中文检阅说明

提交信息保持仓库现有风格：

```text
<type>(<scope>): <lowercase imperative summary>

- describe the first focused change
- describe the second focused change
- describe the resulting behavior or validation
```

规则：

- subject 使用英文、小写动词和明确 scope
- body 使用英文 bullet
- 每个 bullet 以 `-` 开头
- bullet 之间不插入多余空行
- 中文只用于提交前的检阅说明，不写入实际英文 commit message

## 4. 用户确认后逐层提交

确认后按照既定顺序逐个提交，每次只暂存该层涉及的完整文件：

```powershell
git add -- <file-a> <file-b> <file-c>
git commit -m "<subject>" -m "- first bullet`n- second bullet`n- third bullet"
```

提交前如发现空白字符问题，先修正对应文件。若问题已经进入最后一个提交，可在用户允许的范围内 amend，不改变已确认的提交分层和提交信息。

## 5. 提交后验证

全部提交完成后检查：

```powershell
git status --short
git log -<count> --oneline --decorate
git diff --check HEAD~<count>..HEAD
```

确认以下结果：

- 工作区没有意外的未提交改动
- 每个提交的文件范围与检阅方案一致
- commit subject 和 bullet body 与确认版本一致
- 没有 trailing whitespace 或其他 `git diff --check` 问题

## Example

```text
feat(loongarch): extend early UART console support

- initialize the UART device and FIFO
- add UART input and console output helpers
- implement kernel formatted printing
```

检阅阶段使用中文说明提交目的和文件范围；实际 Git 提交信息保持英文。
