# HRCBot（Hurricane Bot）现代化构建版

面向 **Half-Life 2: Deathmatch（hl2dm）** 的机器人插件，基于原始
Hurricane Bot 1.3.4 二进制重建为 **Metamod:Source 1.12** 插件。

每个架构只产出**单个二进制文件**，同时支持旧的 **32 位** 与现代的
**64 位** hl2dm 服务器，覆盖 **Linux 与 Windows**。

---

## 1. 这是什么（请务必先读）

原版 HRCBot 于 2013 年 5 月以**闭源免费软件**形式发布（基于 SDK 2009），
其源码**从未公开**。本包是依据其二进制做的**干净室重建**：

* **确定性层**——插件生命周期、全部 `hrcbot_*` 变量/命令（名称、默认值、
  帮助文本）、机器人名字表与玩家模型表——均按 2013 二进制内嵌字符串与
  RTTI 忠实还原。
* **行为层**——自动地图分析（原 *Dedale*）与寻路（原 *Poseidon*）——是
  **功能性重写**：用引擎 hull 碰撞检测把可行走地面栅格化，再用 A*
  规划路径。它**不是**逐字节翻译，机器人行为细节会与 2013 版不同。
* 旧的 `.hrcbot` 私有位流不可读取。现代版写入新的 `.hrcbot2` 缓存
  （magic `"HRCBOT2"`）；新图首次加载时会自动分析地面。

本环境已验证四个目标**全部可编译通过**，但**未在真实 hl2dm 服务器里进
游戏实测**（也无 Windows/MSVC 环境，Windows 产物靠 CI 首次构建校验）。
请把它当作可用的起点并自行测试反馈。

原作者：**Hurricane**（hurricane.bot@gmail.com）。
原文照见 `docs/original/`（LICENCE/README/LISEZMOI/IMPORTANT）。

---

## 2. 运行需求

* 运行于 2013（Source SDK 2013 / SteamPipe）分支的 **hl2dm 专用服务器**。
  现代 hl2dm 同时提供 32 位与 64 位服务端。
* **Metamod:Source 1.10 / 1.12**（本插件按 1.12 SDK 构建）。先按你的
  系统/架构装好 Metamod:Source。

## 3. 安装

把 `hrcbot-<版本>-linux.tar.gz`（或 `-windows.zip`）解包到 `hl2mp/` 目录：

```
hl2mp/
├── addons/
│   ├── hrcbot_mm.vdf                 # Metamod 插件注册表（不带扩展名）
│   ├── hrcbot_mm_i486.so            # 32 位 Linux
│   ├── hrcbot_mm.x64.so             # 64 位 Linux
│   ├── hrcbot_mm.dll                # 32 位 Windows
│   ├── hrcbot_mm.x64.dll            # 64 位 Windows
│   └── hrcbot_server_plugin/
│       ├── hrcbot_names.txt          # 机器人名字表
│       └── *.hrcbot                 # 旧路径点文件（存档用）
├── README.md / README.zh-CN.md
├── LICENCE
└── docs/original/                   # 2013 原文（许可证要求附带）
```

Metamod 会根据 `hrcbot_mm.vdf` 自动选择正确架构的文件。Windows 只放与你
服务端位数匹配的 `.dll` 即可；Linux 可同时放两份由 MM:S 自动选择。

在 `cfg/server.cfg`（或单独配置）里设置变量，例如：

```
hrcbot_enabled 1
hrcbot_minplayers 0
hrcbot_maxplayers 6
hrcbot_handicap 65
```

重启或 `meta refresh` 生效。

---

## 4. 控制台变量

默认值以原版 1.3.4 已确认项为准。

| 变量 | 默认 | 说明 |
|------|------|------|
| `hrcbot_enabled` | `1` | 0 = 停用且不分析地图。 |
| `hrcbot_minplayers` | `0` | 至少保持 N 名活跃玩家（不足由机器人补）。 |
| `hrcbot_maxplayers` | `5` | 在场玩家达 N 后不再加机器人；多出的机器人会被平衡移除。 |
| `hrcbot_preferredcount` | `0` | 若 >0，在 min/max 之间维持约这么多机器人。 |
| `hrcbot_forceteam` | `-1` | -1 自动平衡，2 联合军，3 反抗军。 |
| `hrcbot_autobalancebots` | `1` | 0 = 关闭自动填充，改用 `hrcbot_add/kick` 手动管理。 |
| `hrcbot_waitforplayers` | `0` | 首个真人连接后才加机器人。 |
| `hrcbot_freezeifnoplayers` | `0` | 最后一名真人离开后踢掉机器人。 |
| `hrcbot_player_spawnweapon` | `smg1` | 出生武器：`smg1 pistol 357 crossbow shotgun ar2 rpg`。 |
| `hrcbot_handicap` | `65` | 瞄准准度惩罚，越大机器人越弱；0 为无惩罚。 |
| `hrcbot_mute` | `0` | 静音机器人语音。 |
| `hrcbot_spawnprotectiontime` | `1800` | 出生保护时间（单位 1/60 秒）。 |
| `hrcbot_spawnprotectionseconds` | `2` | 同上，单位为秒。 |
| `hrcbot_spawnprotectedhealth` | `125` | 血量不低于此值视为受出生保护。 |
| `hrcbot_crowbarmaniacs` | `0` | 让机器人拿撬棍。 |
| `hrcbot_motd` | `0` | 显示宣传 MOTD。 |
| `hrcbot_autoweaponswitch` | `1` | 允许机器人切换新武器。 |
| `hrcbot_playermodel` | `*` | 机器人模型，`*` 随机，或填 `.mdl` 路径。 |
| `hrcbot_dialogmsg` | `1` | 向每个客户端播报插件版本。 |
| `hrcbot_kickcommand` | `kickid` | 踢机器人用的服务端命令（`<命令> <userid>`）。 |
| `hrcbot_notifycriticals` | `0` | 输出详细内部日志。 |
| `hrcbot_namesfile` | `addons/hrcbot_server_plugin/hrcbot_names.txt` | 每行一个机器人名。 |
| `hrcbot_clan` | 空 | 加在机器人名前的战队前缀。 |
| `hrcbot_log` | `0` | 文件日志。 |

## 5. 控制台命令

| 命令 | 说明 |
|------|------|
| `hrcbot_add [2|3]` | 添加机器人（2 联合军 / 3 反抗军）。仅手动模式可用。 |
| `hrcbot_kick [2|3]` | 移除一个机器人（可指定队伍）。仅手动模式可用。 |
| `hrcbot_do "名字" 命令` | 让某个机器人以自身身份执行命令。 |
| `hrcbot_info` | 打印导航/机器人状态用于调试。 |
| `hrcbot_fire` | 调试：让机器人开火。 |
| `hrcbot_move` | 调试移动辅助。 |
| `hrcbot_analyseground` | 强制重新分析当前地图。 |
| `hrcbot_version` | 打印插件版本。 |

团队死斗模式请设 `mp_teamplay 1`。

---

## 6. 从源码构建

### 依赖

* Python 3 与 [AMBuild 2](https://github.com/alliedmodders/ambuild)
  （`python3 -m pip install ambuild`）。
* **hl2sdk**，分支 `hl2dm`：
  `git clone -b hl2dm https://github.com/alliedmodders/hl2sdk`。
* **Metamod:Source**，分支 `1.12-dev`：
  `git clone -b 1.12-dev https://github.com/alliedmodders/metamod-source`。

### Linux（本机，同时编 x86 + x64）

```bash
# 一次性：克隆依赖到 ./deps，安装 32 位多架构工具链
sudo apt-get install gcc g++ gcc-multilib g++-multilib libc6-dev-i386
./build.sh --setup

# 同时构建两个架构
./build.sh all
# 或只构建其一
./build.sh x64
```

产物：`obj-linux/hrcbot_mm.x64/hrcbot_mm.x64.so` 与
`obj-linux/hrcbot_mm_i486/hrcbot_mm_i486.so`。

### Windows

在 MSVC / Visual Studio 环境中（CI 使用 `ilammy/msvc-dev-cmd` 配置）：

```powershell
.\build.ps1 -Setup
.\build.ps1            # 生成 hrcbot_mm.dll 与 hrcbot_mm.x64.dll
```

### 打包

```bash
./package.sh --source            # 在 dist/ 生成源码包
./package.sh --prebuilt obj-linux
```

### 持续集成

`.github/workflows/build.yml` 在每次推送时构建 Linux（x86+x64）与
Windows（x86+x64），产出源码与二进制制品，并在 tag 发布时自动附到
GitHub Release。本仓库默认不自动发布；推送形如 `v*` 的 tag 即可切版。

---

## 7. 说明 / 已知限制

* 当前实现驱动移动，并提供基础的 漫游 / 搜索 / 攻击 状态机；拾取物品、
  跳跃与导航在多数开阔地形可用，但在个别地图上仍需调参。
* 64 位依赖现代 hl2dm 服务端；旧的仅 32 位服务端请用
  `hrcbot_mm_i486.so`。
* 原作者：**Hurricane**。本二进制分析工具与重建为社区存档性质工作。
