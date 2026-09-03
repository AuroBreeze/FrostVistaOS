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

## 2. 按功能和变更块划分提交

- 按功能责任划分提交层次
- 每个提交只包含一个清晰的功能或修复目标
- 普通的单一功能改动按完整文件提交
- 如果多个文件或同一文件包含多个功能混合改动，必须先向用户说明混合范围和 chunk 方案并获得确认
- 获得确认后，首先从当前 `HEAD` 创建并验证备份分支，再使用 patch-based staging 按已确认的 chunk 提交
- chunk 提交必须排除无关 hunks，并在每个 chunk 提交后单独验证
- 普通提交中同一个文件完整归属于一个提交；经确认的混合改动允许按 hunk 跨提交拆分
- 不将用户未确认的改动、临时文件或无关文件带入提交
- 不使用 reset、checkout 等方式覆盖用户现有改动

常见提交边界包括：

- 构建系统、工具链和开发环境配置
- 单个硬件驱动或平台能力
- 启动流程、异常处理或核心逻辑

## 3. 先提供提交方案供检阅

用户确认前，不执行 `git add` 或 `git commit`。先为每一层提供：

- Conventional Commit subject
- 普通提交涉及的完整文件列表；混合改动提交涉及的精确文件、chunk/hunk 范围和变更意图
- 英文 commit message
- 中文检阅说明

如果发现改动混合了多个功能，必须先停止暂存并请求用户确认。确认内容至少包括：混合改动判断、备份分支名称、chunk 顺序、每个 chunk 的文件和 hunk 范围，以及对应的提交信息。

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
- 每个 body bullet 必须控制在 72 个字符以内；超过限制时拆成多条独立 bullet
- 禁止使用续行或手动折行掩盖过长的单条 bullet
- bullet 之间不插入多余空行
- 中文只用于提交前的检阅说明，不写入实际英文 commit message

## 4. 用户确认后逐层提交

对于普通的单一功能提交，确认后按照既定顺序逐个提交，每次只暂存该层涉及的完整文件：

```powershell
git add -- <file-a> <file-b> <file-c>
git commit -m "<subject>" -m "- first bullet`n- second bullet`n- third bullet"
```

对于已经确认的混合改动，必须先创建备份分支，再进行局部暂存：

```powershell
git branch backup/<branch>-wip-YYYYMMDD HEAD
git show-ref --verify --quiet refs/heads/backup/<branch>-wip-YYYYMMDD
git add --patch -- <file>
git commit -m "<subject>" -m "- first bullet`n- second bullet"
```

备份分支保存的是创建时的当前 `HEAD`；未提交的工作区改动仍需保留在工作区，不能在备份分支创建后使用覆盖性命令丢弃。每个 chunk 提交后检查 `git diff --check HEAD~1..HEAD`、工作区状态和必要的构建/测试结果，再继续下一个 chunk。

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
