# FLFHMCL 整合包（`.flfhmclpack`）

`.flfhmclpack` 是 FLFHMCLauncher 的可移植实例包。它保存实例元数据、`.minecraft`、`.hmcl`、模组和可选的运行时 jar，导入后作为一个**隔离实例**运行，不会覆盖全局 `.minecraft` / `.hmcl`。

## 1. 格式

文件是 ZIP 容器，扩展名改为 `.flfhmclpack`。根目录：

```text
flfhmclpack.json
payload/
  minecraft/...      # 实例的 .minecraft
  hmcl/...           # 实例的 .hmcl（含 config/state/cache）
  mods/...           # 散装模组 jar（可选，embedded=false 时不写）
  profile/...        # mclhr / launch / instance 元数据
  runtime/hmcl/...   # 选中的 HMCL jar
  runtime/agents/... # 选中的 Agent jar
```

`flfhmclpack.json` 的核心字段：

```json
{
  "format": "flfhmclpack",
  "schema": 1,
  "generator": { "name": "FLFHMCLauncher", "version": "0.2.0" },
  "createdAt": 1794281657,
  "name": "整合包名称",
  "instance": { "id": "...", "name": "...", "gameVersion": "1.21.1",
                "loader": "fabric", "loaderVersion": "0.19.5",
                "hmclVersion": "", "agentVersion": "", "javaPath": "",
                "isolated": true, "mods": {"mode": "private"},
                "minecraft": {"mode": "private"}, "hmclHome": {"mode": "private"} },
  "directories": [
    { "path": ".minecraft/config", "role": "minecraft" },
    { "path": ".hmcl/config", "role": "hmcl" }
  ],
  "files": [
    { "path": "options.txt", "role": "minecraft", "size": 1234, "sha256": "..." },
    { "path": "hmcl/HMCL-3.16.4.jar", "role": "runtime",
      "target": "hmcl/HMCL-3.16.4.jar", "size": 0, "sha256": "..." }
  ],
  "mods": [
    { "path": "foo.jar", "role": "mods", "file": "foo.jar",
      "size": 1234, "sha256": "...", "embedded": true, "url": "" },
    { "path": "bar.jar", "role": "mods", "file": "bar.jar",
      "size": 1234, "sha256": "...", "embedded": false,
      "url": "https://example.com/bar.jar" }
  ]
}
```

- `role=minecraft` → 导入到实例的 `.minecraft`；
- `role=hmcl` → 导入到实例的 `.hmcl`；
- `role=mods` → 导入到实例隔离的 `mods/`；
- `role=profile` → 导入到 `.flfhmcl/profile/<uuid>/`；
- `role=runtime` → 按 `target` 放回 `.flfhmcl/hmcl` 或 `.flfhmcl/agents`。

## 2. 模组两种形态

- **嵌入**：`embedded=true`，jar 在 `payload/mods/` 中，随包分发；
- **链接**：`embedded=false`，`url` 指向 jar；导入时下载并校验 `sha256`。

当前启动器导出默认嵌入 jar；链接模式由 core API 的 `ModpackOptions::embedMods=false` +
`modLinks` 提供，适合整合包作者只分发清单、不让 jar 进包。

## 3. 使用

### 拖拽

把 `.flfhmclpack` 拖到 FLFHMCLauncher 窗口即可导入。导入后：

- 自动生成新 uuid / 唯一 id；
- 强制 `isolated=true`，`.minecraft` 与 `.hmcl` 放在 `.launcher/<uuid>/`；
- 恢复所有目录、文件、空文件夹和运行时 jar；
- 逐个校验 SHA-256；
- 写入 `instances.json` 并选中新实例。

### 按钮

启动页左侧「导入整合包」/「导出当前实例」。

### 打开方式

不需要注册。若你把 `.flfhmclpack` 指定为用 `FLFHMCLauncher.exe` 打开，程序会从命令行识别并导入：

```text
FLFHMCLauncher.exe "D:\packs\example.flfhmclpack"
```

图标资源：

- `assets/modpack/flfhmclpack.ico`
- exe 内资源 ID `102`（`resources/flfhmlauncher.rc`）
- 安装目录下的 `flfhmclpack.ico`

可选关联（不执行也能拖拽/按钮导入）：

```powershell
$exe = "C:\Path\FLFHMCLauncher.exe"
$ico = "C:\Path\flfhmclpack.ico"
New-Item -Path "HKCU:\Software\Classes\.flfhmclpack" -Force | Out-Null
Set-ItemProperty "HKCU:\Software\Classes\.flfhmclpack" -Name "(Default)" -Value "FLFHMCLauncher.flfhmclpack"
New-Item -Path "HKCU:\Software\Classes\FLFHMCLauncher.flfhmclpack\DefaultIcon" -Force | Out-Null
Set-ItemProperty "HKCU:\Software\Classes\FLFHMCLauncher.flfhmclpack\DefaultIcon" -Name "(Default)" -Value "`"$exe`",-102"
New-Item -Path "HKCU:\Software\Classes\FLFHMCLauncher.flfhmclpack\shell\open\command" -Force | Out-Null
Set-ItemProperty "HKCU:\Software\Classes\FLFHMCLauncher.flfhmclpack\shell\open\command" -Name "(Default)" -Value "`"$exe`" `"%1`""
```

## 4. 安全与限制

- 导入时拒绝绝对路径和 `..` 路径；
- 不跟随符号链接；
- 所有文件校验 SHA-256；
- `.hmcl` 可能包含账户 token / 登录状态，分发前请自行清理；
- 当前导出会包含选中实例的 `.minecraft` / `.hmcl` 全部内容（包括日志、缓存），大实例会很大；
- 整合包不包含 `libraries/` / `java/`；这些由启动器按元数据补齐。
