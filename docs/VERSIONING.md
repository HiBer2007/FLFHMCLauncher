# 文档多版本约定（VERSIONING）

> 适用：FLFHMCLauncher 文档（本仓库 `docs/`）
> 依据：FLFHMCL 平台设计 `PLAN.md` §7（文档多版本）、§3（目录契约）；`PLAN.md` 以 FLFHMCL 工作区根目录版本为准
> 单一事实源：`docs/versions.json`

FLFHMCL、FLF API、FLFHMCLauncher、mclhr 四个文档库各有自己的 `docs/versions.json`，互不共用；本文件只描述本仓库的约定。

## 一、当前版本与归档版本

| 线 | 是什么 | 存放在哪 | 可否修改 |
|---|---|---|---|
| 当前版本 | 正在维护的最新文档，当前为 `0.3.12` | 文档分支 `main` 上的 `docs/` | 可以，按 `docs/_AUTHORING.mdx` 与 PLAN 改 |
| 归档版本 | 历史版本快照：`0.3.11` | 归档 ref `docs-<version>` 上的同一个 `docs/` | 冻结，只允许补“已过时”标记 |

- 工作树里永远只有一份页面：`docs/zh/**` 与 `docs/en/**`，中英同 slug 一一镜像。
- 历史版本不以目录形式出现在工作树里：没有 `docs/0.3.11/` 这类路径。
- 版本号只是标识符，不进入页面路径。
- 当前版本 ref 是 `main`，归档 ref 是 `docs-0.3.11`；这些名字必须与 `versions.json` 里的 `ref` 逐字一致。
- 版本号唯一来源是 `CMakeLists.txt` 的 `project(FLFHMCLauncher VERSION ...)`；三段普通版本，不按 FLF API 的四段规则改写。

## 二、`docs/versions.json` 是版本选择器的唯一数据源

站点/主题的版本下拉框只读这个文件：不扫目录、不读页面 frontmatter、不猜分支名。字段与规则：

| 字段 | 含义 | 规则 |
|---|---|---|
| `current` | 当前文档版本号 | 必须等于 `versions[]` 中 `status` 为 `current` 那条的 `id` |
| `versions[]` | 选择器条目 | 顺序即展示顺序：当前在前，归档按版本号降序 |
| `versions[].id` | 版本号 | 全清单唯一，同一版本不得出现两次 |
| `versions[].label` | 展示名 | 固定 `<产品> <版本号>`，例如 `FLFHMCLauncher 0.3.11` |
| `versions[].status` | 状态 | 只有 `current`、`archived` 两个取值；`current` 有且只有一条 |
| `versions[].ref` | 该版本页面所在的 Git ref | 当前版本写文档分支名，归档写 `docs-<version>` |
| `versions[].docsPath` | 文档根目录 | 相对仓库根，恒为 `docs` |
| `versions[].locales` | 该版本提供的语言 | `zh`、`en`，两者必须成对提供 |
| `convention` | 约定本身 | 供脚本与代码评审校验 |

清单损坏的判定（任一条不满足）：

1. 文件不是纯 JSON：带注释、带尾逗号或无法被 `ConvertFrom-Json` / `JSON.parse` 解析。
2. `current`、当前条目的 `id`、`convention.currentRef`（等于当前条目的 `ref`）三者不一致。
3. 归档条目的 `ref` 不等于 `docs-<id>`（`convention.archiveRefPattern` 里的 `<version>` 就是 `id`）。
4. 已有条目的 `id` 或 `ref` 被改名——归档只新增，不改名。

## 三、归档是 Git ref，不是复制的目录

归档版本 = 仓库里的一个 ref，名字固定 `docs-<version>`：

```text
git tag docs-0.3.11 <0.3.11 最后一次发布文档的提交>   # 不可变，推荐
git branch docs-0.3.11 <同一提交>                     # 允许后续补“已过时”标记
git push origin docs-0.3.11
```

为什么不复制目录：

- 复制出来的 `docs/<version>/...` 会和当前页面**路由碰撞**：同一 slug 在 Nextra 里出现两次，`_meta.js`、图片相对路径、站内链接、sitemap 与搜索索引都要再做一份。
- 副本没有权威性：在副本上改字会让“历史版本”变成第二份“当前文档”，PLAN §7 要求的“不得覆盖当前语义”就失效了。
- Git ref 天然按版本隔离，tag 天然不可变；比较两个版本只要 `git diff docs-0.3.11..main -- docs`。

为什么 ref 名用连字符 `docs-<version>` 而不是 `docs/<version>`：

- `docs/<version>` 会占用 `docs/` 这个 ref 目录，与其它 `docs/*` 分支名在**同一个 ref 命名空间里碰撞**，`git` 会报 `cannot lock ref`。
- `docs-<version>` 是平级名字，与 `main` 并存不冲突；每个版本一个 ref，互不覆盖。
- 版本号里的 `.` 在 ref 名里合法，可以直接用 `docs-0.3.11`。
- 归档 ref 只增不改：已写进选择器的名字不再分配给别的版本，也不 `push --force`；与已有的 `v0.3.*` 发布 tag 是两套名字，不互相改写。

ref 在所属仓库里解析：本仓库即 FLFHMCLauncher 仓库。

## 四、过时语义必须显式标记，禁止复用

PLAN §3 定稿后的目录契约：

| 目录 | 语义 |
|---|---|
| `<实例>/.hmcl` | HMCL 工作目录，也就是 Fabric 的 `--gameDir` |
| `<实例>/.minecraft` | HMCL 要启动的 Minecraft 实例，通过 `-Dflfhmcl.gameDir` 传给 Agent |
| `<实例>/.flfhmcl` | FLFHMCL 私有运行时（`-Dflf.home`、`-Dflf.modsDir`） |

这三个目录禁止互换。归档版本（`0.3.11`）里的页面只要还写着“`--gameDir` 就是 `.minecraft`”“HMCL 工作目录是 `.minecraft`”“在 `.flfhmcl` 下再造一个 HMCL 运行时目录”之类的旧说法，就必须显式标为**已过时**（obsolete）：

1. 在归档 ref 内的页面顶部加可见标记，二选一：
   - frontmatter 加 `obsolete: true`；
   - 正文第一行写 `> ⚠️ 已过时：本页描述的是 <版本> 的旧 GameDir 语义，已被 PLAN §3 取代，请勿照抄。`
2. 整个版本都被取代时，在归档 ref 的 `docs/index.mdx` 顶部加整版说明。
3. 这些旧页面**不得复用**：不得把正文复制回当前分支，不得拿归档页当新页模板，不得引用其中的目录约定；当前分支只以 `PLAN.md` 和源码为准。
4. 归档 ref 里只允许加“过时标记”，不改正文语义；要修语义就在当前分支修。

> 备注：`0.3.11` 是否含旧说法要逐页核对，含旧说法的页面按本节标记后再冻结。

## 五、新增一个版本

发布新的当前版本时按顺序做：

1. **先归档旧当前版本**：在旧版本最后一次发布文档的提交（通常是发布 tag）上建 ref：`git tag docs-<旧版本> <提交>`，然后 `git push origin docs-<旧版本>`。归档内容就是该提交里的 `docs/`，不复制到工作树。
2. **补过时标记**（需要时）：只在归档 ref 上加 §四 的标记，提交后保持冻结。
3. **更新当前分支页面**：把 `docs/{zh,en}/**/*.mdx` frontmatter 的 `version:` 改成新版本号；页面仍是当前版本的唯一副本。
4. **更新 `docs/versions.json`**：
   - `current` 改成新版本号；
   - 新版本条目放最前，`status` 为 `current`，`ref` 写当前文档分支；
   - 原当前条目改成 `status: "archived"`、`ref: "docs-<旧版本>"`；
   - 归档条目按版本号降序排列；
   - 文档分支改名时同步 `convention.currentRef` 与当前条目的 `ref`。
5. **校验**：JSON 可解析、`id` 唯一、`current` 与当前条目一致、每个 `ref` 都能 `git rev-parse --verify`（§七）。
6. **同一次提交里**同时改页面 `version` 与 `versions.json`，避免选择器和页面版本错位。

## 六、页面 `version` 与清单的分工

- 页面 frontmatter 的 `version:` 是**页面元数据**：声明这页面向哪个版本，便于脚本做一致性检查。
- **选择器**只认 `docs/versions.json`：加语言、调顺序、加归档都只改清单，不改页面。
- 两者不一致时以清单为准（它是选择器的事实源），并在下一次文档提交里把页面 `version` 对齐到 `current`。
- 不要为了“多版本”给页面加 `versions:` 之类的 frontmatter 数组，也不要用 URL 前缀（`/v0.3.11/...`）实现版本切换。

## 七、校验清单

```powershell
$m = Get-Content docs/versions.json -Raw | ConvertFrom-Json
$current = @($m.versions | Where-Object status -eq 'current')

# 1) 结构与当前版本
$m.current -eq $current[0].id                    # True
$current.Count -eq 1                             # True
$m.convention.currentRef -eq $current[0].ref     # True

# 2) id、ref 唯一
$ids = @($m.versions.id)
($ids | Sort-Object -Unique).Count -eq $ids.Count
$refs = @($m.versions.ref)
($refs | Sort-Object -Unique).Count -eq $refs.Count

# 3) 归档命名、docsPath、locales
$m.versions | Where-Object { $_.status -eq 'archived' } |
  ForEach-Object { $_.ref -eq "docs-$($_.id)" }
$m.versions | ForEach-Object { $_.docsPath -eq 'docs' -and ($_.locales -join ',') -eq 'zh,en' }

# 4) ref 必须真实存在（缺失先按 §五 建 ref）
$m.versions | ForEach-Object { git rev-parse --verify --quiet $_.ref }

# 5) 没有把归档复制成目录
Get-ChildItem docs -Directory | Where-Object Name -Match '^\d+\.'   # 必须为空
```

## 八、常见问题

- **能不能直接建 `docs/0.3.11/` 放旧页面？** 不能，会和当前页撞路由，也违背 PLAN §7。
- **归档版本要不要跟着改正文？** 不要，归档只补“已过时”标记。
- **归档 ref 还没建，选择器会怎样？** 该版本链接 404；发布清单前必须跑 §七 第 4 项。
- **新旧版本 ref 名会撞吗？** 不会：每个版本一个平级名字 `docs-<version>`，只新增不改名。
- **会和已有 tag `v0.3.10` 撞吗？** 不会：发布 tag 是 `v0.3.*`，归档 ref 是 `docs-0.3.11`，两套名字互不覆盖。
- **`main` 上还能看到旧版本页面吗？** 看不到，旧页面只存在于归档 ref。
