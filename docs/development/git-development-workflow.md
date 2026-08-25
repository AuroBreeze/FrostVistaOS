# Git 日常开发流程

本文档记录 FrostVistaOS 的日常 Git 开发流程，重点说明特性分支、跨设备开发、WIP checkpoint，以及将整理后的改动合入 `dev` 的方式。

## 0. 完整流程

日常开发、WIP 整理和合入 `dev` 应按以下顺序执行：

1. 获取远程状态，确认当前分支、上游关系和工作区状态。
2. 判断当前提交历史中是否包含 WIP，以及 WIP 前是否已经存在完整提交。
3. 如果存在 WIP，先创建备份分支；优先使用 `git reset --mixed` 整理 WIP，保留工作区文件改动并清空暂存区。
4. 如果 WIP 底部已有完整提交，只回滚到最后一个完整提交，不要把完整提交一并回滚。完整提交通常位于 WIP 之前的提交历史底部，不应夹在连续的 WIP 提交中间。
5. 检查整理后的工作区、完整 diff、空白字符和构建/测试结果。
6. 进入提交阶段后，必须同时遵循 [`git-commit-workflow.md`](git-commit-workflow.md)：检查工作区和提交风格，按完整文件划分提交，先提供提交方案；用户确认后，才执行 `git add` 和 `git commit`。
7. 在特性分支上验证提交历史和工作区，然后使用 `git push --force-with-lease` 同步重写后的特性分支。
8. 合入 `dev` 前重新获取远程状态；根据分支保护规则直接推送或创建 Pull Request。

如果完整提交与 WIP 交错、无法明确划分边界，先停止 reset，使用 `git log --oneline --decorate --all` 和备份分支确认提交关系，不要盲目回滚。

## 1. 分支职责

```text
dev
  └── feature/loongarch
        ├── 日常开发和 WIP checkpoint
        └── 整理后合入 dev
```

- `dev` 是共享集成分支，放置已经整理并完成基本验证的改动。
- `feature/loongarch` 是个人特性分支，可以包含 WIP commit，用于持续开发和跨设备同步。
- 个人特性分支可以重写历史；`dev` 不应强制推送。

特性分支应当跟踪对应的远程特性分支：

```text
feature/loongarch -> origin/feature/loongarch
dev               -> origin/dev
```

不要让 `feature/loongarch` 跟踪 `origin/dev`，否则在特性分支上执行普通 `git push` 时，可能误更新 `dev`。

## 2. 开始工作

在当前设备开始开发前，先同步远程分支：

```powershell
git fetch origin
git switch feature/loongarch
git pull --ff-only
```

确认当前分支和工作区状态：

```powershell
git status --short --branch
git branch -vv
```

如果工作区存在未确认的改动，不要直接执行覆盖性操作。先确认这些改动是否属于当前任务。

## 3. 保存 WIP checkpoint

当完成一个适合跨设备继续的阶段时，保存一个 WIP checkpoint：

```powershell
git status --short
git add -- <file-a> <file-b>
git commit -m "WIP: checkpoint <short description>"
git push
```

WIP commit 的目标是保存可恢复的开发状态，不要求立即形成最终提交结构。建议：

- 使用明确的文件列表，避免把临时文件和生成文件带入提交；
- 在 commit message 中说明 checkpoint 的功能范围；
- 每完成一个较大的逻辑阶段就推送一次；
- 不要把 `dev` 作为个人 checkpoint 的远程目标。

下一台设备继续开发时，重复“开始工作”中的同步步骤即可。

WIP checkpoint 也是一次 Git 提交，执行前同样必须遵循 [`git-commit-workflow.md`](git-commit-workflow.md) 中的检查、完整文件暂存和提交后验证要求。WIP 可以使用 `WIP: checkpoint <short description>` 作为提交主题，但不能因此跳过工作区检查或文件范围确认。

## 4. 恢复远程特性分支

如果 Git 显示远程特性分支已经不存在：

```text
origin/feature/loongarch [gone]
```

可以使用当前本地分支重新创建远程特性分支，并恢复上游关系：

```powershell
git push -u origin HEAD:feature/loongarch
```

然后确认：

```powershell
git branch -vv
```

预期结果：

```text
feature/loongarch ... [origin/feature/loongarch]
dev               ... [origin/dev]
```

删除本地备份分支通常不会删除远程特性分支。如果远程分支确实消失，应检查远程分支删除记录、仓库权限和分支保护规则。

## 5. 整理 WIP 历史

准备将 WIP 整理成正式 commit 时，先为当前状态建立备份分支：

```powershell
git switch feature/loongarch
git branch backup/loongarch-wip-YYYYMMDD HEAD
```

备份分支不会复制文件，只会保存当前 commit 的引用。即使后续 reset 或整理失败，也可以通过它找回原始 WIP 历史。

确认备份分支已经创建后，先检查 WIP 和完整提交的边界：

```powershell
git log --oneline --decorate --graph origin/dev..feature/loongarch
git log --oneline --decorate --all -12
```

通常提交顺序如下：

```text
origin/dev
  └── 完整提交 A
        └── 完整提交 B
              └── WIP checkpoint 1
                    └── WIP checkpoint 2   <- feature/loongarch
```

这种情况下，优先使用 `git reset --mixed` 回滚 WIP，目标是最后一个完整提交 `B`，而不是直接回滚到 `origin/dev`：

```powershell
git reset --mixed <last-complete-commit>
```

如果 WIP 之前没有完整提交，才回滚到 `origin/dev`：

```powershell
git fetch origin
git reset --mixed origin/dev
```

`--mixed` 的效果是：

- 当前分支的提交历史回到指定的完整提交或 `origin/dev`；
- 工作区中的 WIP 改动全部保留；
- 暂存区被清空；
- 可以按完整文件重新选择正式 commit 的边界。

`--mixed` 是整理 WIP 的首选方式。它不会覆盖工作区文件，也不会删除未跟踪文件；它只移动当前分支指针并清空暂存区。不要使用 `reset --hard` 代替它。

完整提交大概率位于 WIP 提交的底部，并且不夹在 WIP 中间。只要完整提交的边界明确，就必须保留这些提交，不要为了整理 WIP 把它们重新变成工作区改动。

检查回滚结果：

```powershell
git status --short
git diff --stat
```

之后按照功能责任逐组选择文件并提交：

整理后的正式提交必须切换到 [`git-commit-workflow.md`](git-commit-workflow.md) 流程。每一层提交前都要先确认完整文件列表、Conventional Commit subject、英文 body 和中文检阅说明；用户确认前不得执行 `git add` 或 `git commit`。

```powershell
git add -- <file-a> <file-b>
git commit -m "feat(loongarch): add early UART console support"
```

整理过程中如果发现方向不对，可以通过备份分支恢复原始 WIP 状态：

```powershell
git reset --hard backup/loongarch-wip-YYYYMMDD
```

只有在确认工作区没有需要保留的未提交改动时，才可以使用 `reset --hard`。如果不确定，应先检查 `git status --short`。

如果不需要保留备份分支，可以在整理完成并合入 `dev` 后删除本地备份：

```powershell
git branch -d backup/loongarch-wip-YYYYMMDD
```

如果 WIP 改动需要跨设备恢复，也可以把备份分支推送到远程：

```powershell
git push -u origin backup/loongarch-wip-YYYYMMDD
```

完成回滚并重新提交后，再同步最新的 `dev`：

```powershell
git fetch origin
git rebase origin/dev
```

整理后的提交应按功能责任划分，例如：

```text
fix(build): improve cross-platform build support
feat(loongarch): extend early UART console support
feat(loongarch): add early trap bring-up scaffold
```

实验性代码应使用明确的描述。仍然包含主动故障触发、无限循环处理器或不完整寄存器保存的 bring-up 改动，不应使用 `complete` 等表示功能已经完成的措辞。

整理个人特性分支后，使用租约保护的强制推送：

```powershell
git push --force-with-lease origin feature/loongarch
```

`--force-with-lease` 可以避免覆盖本地尚未看到的远程提交。不要对 `dev` 使用强制推送。如果其他开发者开始使用该特性分支，就停止重写历史，改用新的整理分支或 Pull Request。

## 6. 合入 `dev`

合入前检查远程状态、提交历史和工作区：

```powershell
git fetch origin
git diff --check origin/dev
git log --oneline origin/dev..feature/loongarch
git status --short
```

如果允许直接推送到 `dev`，并且 `dev` 没有新的提交，可以显式推送：

```powershell
git push origin feature/loongarch:dev
```

如果 `dev` 受到保护，推荐创建临时整理分支并提交 Pull Request：

```powershell
git switch -c loongarch-ready origin/dev
git merge --squash feature/loongarch
git commit -m "feat(loongarch): add early bring-up support"
git push -u origin HEAD:loongarch-ready
```

这样可以保留完整 WIP 历史作为恢复点，同时让 `dev` 只接收经过选择和整理的改动。

## 7. 安全检查与恢复

重写历史前，先检查远程和本地状态：

```powershell
git fetch origin
git log --oneline --decorate --all -12
git status --short
```

如果整理历史出现问题，可以通过 reflog 找回特性分支之前的位置：

```powershell
git reflog feature/loongarch
```

重要里程碑可以创建并推送不可变 tag：

```powershell
git tag checkpoint/loongarch-YYYYMMDD
git push origin checkpoint/loongarch-YYYYMMDD
```

每次推送前确认：

- 当前分支是预期的特性分支；
- 上游是 `origin/feature/loongarch`，而不是 `origin/dev`；
- `git status --short` 中没有意外文件；
- `git diff --check` 没有空白字符错误；
- 只有个人特性分支使用 `--force-with-lease`。
