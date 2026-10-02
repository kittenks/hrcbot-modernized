# HRCBot (Hurricane Bot) — modernized build

A bot plugin for **Half-Life 2: Deathmatch (hl2dm)**, rebuilt as a
**Metamod:Source 1.12** plugin from the original Hurricane Bot 1.3.4 binary.

It ships **single per-architecture binaries** and supports both the legacy
32-bit and the modern 64-bit hl2dm server, on **Linux and Windows**.

---

## 1. What this is (please read)

The original HRCBot was released as a **closed-source, freeware** binary in
May 2013 (built against SDK 2009). Its source was **never published**, so this
package is a **clean-room reconstruction**:

* Deterministic surface — plugin lifecycle, all `hrcbot_*` convars/commands
  (names, defaults, help text), the name list and player-model tables — is
  reconstructed faithfully from the 2013 binary's embedded strings and RTTI.
* Behavioural surface — the automatic map analysis (original *Dedale*) and
  pathfinding (original *Poseidon*) — is a **functional reimplementation**
  using engine hull traces to rasterize walkable space and A* to route bots.
  It is **not** a byte-exact translation and bot behaviour will differ in
  detail from 2013.
* The old `.hrcbot` private bitstream is unreadable. The modern build writes
  a new `.hrcbot2` cache (`magic "HRCBOT2"`); on a new map it analyses the
  ground automatically the first time the map is loaded.

Builds are verified to **compile** for all four targets. They have **not**
been play-tested inside a live hl2dm server from this environment. Treat it
as a starting point and give feedback.

Original authorship: **Hurricane** (hurricane.bot@gmail.com). See
`docs/original/` for the verbatim LICENCE/README/LISEZMOI/IMPORTANT.

---

## 2. Requirements

* A Half-Life 2: Deathmatch **dedicated server** on the 2013 (Source SDK
  2013 / SteamPipe) branch. Modern hl2dm ships both 32-bit and 64-bit builds.
* **Metamod:Source 1.10 / 1.12** (the plugin is built against the 1.12 SDK).
  Install Metamod:Source for your OS/arch first.

## 3. Installing the binary package

Unpack `hrcbot-<version>-linux.tar.gz` (or `-windows.zip`) into your
`hl2mp/` directory so that you get:

```
hl2mp/
├── addons/
│   ├── hrcbot_mm.vdf                 # Metamod:Source plugin registry
│   ├── hrcbot_mm_i486.so            # 32-bit Linux plugin
│   ├── hrcbot_mm.x64.so             # 64-bit Linux plugin
│   ├── hrcbot_mm.dll                # 32-bit Windows plugin
│   ├── hrcbot_mm.x64.dll            # 64-bit Windows plugin
│   └── hrcbot_server_plugin/
│       ├── hrcbot_names.txt          # bot name list
│       └── *.hrcbot                 # legacy waypoints (kept for reference)
├── README.md
├── README.zh-CN.md
├── LICENCE
└── docs/original/                   # original 2013 text files (required)
```

Metamod:Source picks the correct architecture file automatically from the
`hrcbot_mm.vdf` (which has no extension). On Windows you only need the `.dll`
matching your server build; on Linux you can ship both and MM:S selects the
right one.

Add to your `cfg/server.cfg` (or a dedicated config) the cvars you want, e.g.:

```
hrcbot_enabled 1
hrcbot_minplayers 0
hrcbot_maxplayers 6
hrcbot_handicap 65
```

Restart or `meta refresh`.

---

## 4. Console variables

Defaults match the original 1.3.4 where confirmed.

| CVar | Default | Meaning |
|------|---------|---------|
| `hrcbot_enabled` | `1` | 0 = disable all activity and skip map analysis. |
| `hrcbot_minplayers` | `0` | Keep at least N active players (bots fill in). |
| `hrcbot_maxplayers` | `5` | No more bots once N players are present; bots balance down to this. |
| `hrcbot_preferredcount` | `0` | If >0, keep roughly this many bots within min/max bounds. |
| `hrcbot_forceteam` | `-1` | -1 balanced, 2 combine, 3 rebels. |
| `hrcbot_autobalancebots` | `1` | 0 disables automatic population; use `hrcbot_add/kick` manually. |
| `hrcbot_waitforplayers` | `0` | Bots only added after the first human connects. |
| `hrcbot_freezeifnoplayers` | `0` | Drop bots when the last human leaves. |
| `hrcbot_player_spawnweapon` | `smg1` | `smg1 pistol 357 crossbow shotgun ar2 rpg`. |
| `hrcbot_handicap` | `65` | Aim skill penalty (higher = weaker bots; 0 = no handicap). |
| `hrcbot_mute` | `0` | Mute bots' chatter. |
| `hrcbot_spawnprotectiontime` | `1800` | Leave spawned players alone (1/60 s units). |
| `hrcbot_spawnprotectionseconds` | `2` | Convenience: sets the value above in seconds. |
| `hrcbot_spawnprotectedhealth` | `125` | Assume a player with at least this much health is spawn-protected. |
| `hrcbot_crowbarmaniacs` | `0` | Give bots a crowbar. |
| `hrcbot_motd` | `0` | Show the "advertising" MOTD. |
| `hrcbot_autoweaponswitch` | `1` | Let bots switch to new weapons. |
| `hrcbot_playermodel` | `*` | Player model (`*` = random; or a `.mdl` path). |
| `hrcbot_dialogmsg` | `1` | Announce the bot version on each client. |
| `hrcbot_kickcommand` | `kickid` | Server command used to remove a bot (`<cmd> <userid>`). |
| `hrcbot_notifycriticals` | `0` | Verbose internal logging. |
| `hrcbot_namesfile` | `addons/hrcbot_server_plugin/hrcbot_names.txt` | One bot name per line. |
| `hrcbot_clan` | `` | Prefix prepended to each bot name. |
| `hrcbot_log` | `0` | File logging. |

## 5. Console commands

| Command | Meaning |
|---------|---------|
| `hrcbot_add [2|3]` | Add a bot (team 2 = combine, 3 = rebels). Manual mode only. |
| `hrcbot_kick [2|3]` | Remove a bot (optionally from a team). Manual mode only. |
| `hrcbot_do "name" command` | Make a bot run a command as itself. |
| `hrcbot_info` | Dump navigation / bot state for debugging. |
| `hrcbot_fire` | Debug: make bots fire. |
| `hrcbot_move` | Debug movement helper. |
| `hrcbot_analyseground` | Force a re-analysis of the current map. |
| `hrcbot_version` | Print the plugin version. |

For team deathmatch, set `mp_teamplay 1`.

---

## 6. Building from source

### Dependencies

* Python 3 + [AMBuild 2](https://github.com/alliedmodders/ambuild)
  (`python3 -m pip install ambuild`).
* **hl2sdk**, branch `hl2dm` — `git clone -b hl2dm https://github.com/alliedmodders/hl2sdk`.
* **Metamod:Source**, branch `1.12-dev` —
  `git clone -b 1.12-dev https://github.com/alliedmodders/metamod-source`.

### Linux (native, builds x86 + x64)

```bash
# one-time, clones deps into ./deps and installs multilib
sudo apt-get install gcc g++ gcc-multilib g++-multilib libc6-dev-i386
./build.sh --setup

# build both architectures
./build.sh all
# or a single one
./build.sh x64
```

Result: `obj-linux/hrcbot_mm.x64/hrcbot_mm.x64.so` and
`obj-linux/hrcbot_mm_i486/hrcbot_mm_i486.so`.

### Windows

From an MSVC / Visual Studio environment (the CI uses `ilammy/msvc-dev-cmd`):

```powershell
.\build.ps1 -Setup
.\build.ps1            # builds hrcbot_mm.dll and hrcbot_mm.x64.dll
```

### Package

```bash
./package.sh --source            # source tarball in dist/
./package.sh --prebuilt obj-linux
```

### CI

`.github/workflows/build.yml` builds Linux (x86+x64) and Windows (x86+x64) on
every push, produces source and binary artifacts, and attaches them to GitHub
Release tags. (This repository does not auto-publish; push a tag `v*` to cut a
release after you enable the workflow.)

---

## 7. Notes / known limitations

* The plugin currently drives movement and a basic roam/hunt/attack state
  machine; item pickup, jumps and navigation cover most open ground but will
  need tuning on specific maps.
* 64-bit support relies on the modern hl2dm server; older 32-bit-only servers
  use `hrcbot_mm_i486.so`.
* Original credits: **Hurricane**. The binary analysis tooling and this
  reconstruction are community preservation work.
