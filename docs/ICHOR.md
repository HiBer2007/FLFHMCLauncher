# ichor 调研记录

> 结论：**ichor 是 Lunar Client 私有的类加载 / 模块化运行时**，非公开标准、无独立官方仓库，
> 由 `genesis-*.jar` 等核心 jar 内的 `com.moonsworth.lunar.ichor.*` 实现，运行期数据在
> `.lunarclient/offline/multiver/.ichor/`。

## 证据

| 证据 | 路径 | 说明 |
|------|------|------|
| 核心类 | `genesis-0.1.0-SNAPSHOT-all.jar` | `com/moonsworth/lunar/ichor/IchorClassLoader`、`IchorPipeline` 等（类名混淆） |
| 共享库仓 | `.lunarclient/offline/multiver/.ichor/ichormodule-libs/` | `net_fabricmc_fabric_loader_0_20_1-lunar-0.1.5.jar` 这类扁平命名 |
| 运行日志 | `.ichor/genesis.log` | `Found classpath URL ... with SHA-1 hash`、`Baking classes... Baked 11607 classes in 27896ms`、`Missing class cache` |
| 启动日志 | `profiles/1.21/logs/ichor-boot.log` | `IchorPipeline` 为 `META_MIXIN` / `MIXIN` 分别加载 Mixin；`GenesisClassLoader(v1_21_11)` |
| 服务入口 | `META-INF/services/com.moonsworth.lunar.ichor.*` | ServiceLoader 模块发现 |

## 行为归纳（不复制实现）

1. 多层 ClassLoader：meta / mixin / 游戏层，每层独立 Mixin runtime；
2. 共享库仓：`ichormodule-libs/<group>_<artifact>_<version>[-<vendor>].jar`，多实例共用；
3. 类烘焙缓存：`cache/<hash>/bake.cache`，加速二次启动；
4. 模块清单驱动装配（依赖库 + Mixin 配置）。

## mclhr（我们的等价实现，已开源）

- 共享库仓：`.flfhmcl/libraries/`，同上扁平命名；
- 仓库：<https://github.com/HiBer2007/mclhr>；
- 实例清单：`instance.json` 保存 Maven 坐标 + SHA-1；
- FLFHMCL agent 解析清单 → 校验 → 拼 classpath；
- 分层隔离：Java / JavaFX / Fabric / HMCL / 库 / mods / .minecraft / .hmcl 可分别选择
  私有、全局共享或组共享（见 `PLAN.md` §4）。

> 不复用 Lunar 的闭源代码；如需与 Lunar 行为对齐，以黑盒方式验证启动结果即可。
