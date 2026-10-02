#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
HRCBot (Hurricane Bot) static analysis / reverse-engineering helper.

This tool does NOT magically turn a stripped, optimized C++ binary back into the
original hand-written sources.  No tool can do that reliably: templates, inlined
functions, class layouts and STL internals are largely erased during compilation.

What it DOES do is parse both shipped binaries (the Win32 PE .dll and the
Linux ELF .so) and recover every piece of information that is still present in
the machine code, producing a deterministic "blueprint" that can be used to
reconstruct a modern, compilable source tree:

  * PE / ELF headers, architecture, build timestamps
  * exported and imported symbols (engine / tier0 / vstdlib surface)
  * MSVC (.?AV) and Itanium (_ZTI/_ZTV) RTTI class names, demangled
  * Source engine interface version strings (VEngineServer021, ...)
  * every hrcbot_* ConVar / ConCommand name, paired with its help text
  * the ordered read-only string table (chat lines, log lines, format strings)
  * source file + line anchors recovered from the assert ("THROW") strings
  * the .hrcbot waypoint container header and the bot-name table

Outputs (next to --out, default ./analysis):
  blueprint.json   machine readable result
  BLUEPRINT.md     human readable report
  strings_*.txt    ordered printable strings per binary for manual review

Usage:
  python3 hrcbot_analyze.py --pkg <extracted hrcbot folder> [--out analysis]
"""

import argparse
import json
import os
import re
import struct
import subprocess
import sys
from collections import OrderedDict

# ---------------------------------------------------------------------------
# Small, dependency-tolerant binary helpers
# ---------------------------------------------------------------------------

PRINTABLE = set(range(0x20, 0x7F))


def extract_strings(data, min_len=4):
    """Return [(file_offset, ascii_string), ...] in file order."""
    out = []
    start = None
    buf = bytearray()
    for i, b in enumerate(data):
        if b in PRINTABLE or b in (9,):
            if start is None:
                start = i
                buf = bytearray()
            buf.append(b)
        else:
            if start is not None and len(buf) >= min_len:
                out.append((start, bytes(buf).decode("ascii", "replace")))
            start = None
            buf = bytearray()
    if start is not None and len(buf) >= min_len:
        out.append((start, bytes(buf).decode("ascii", "replace")))
    return out


def demangle_simple(name):
    """Best-effort Itanium demangle via c++filt, fall back to the raw name."""
    if not name.startswith("_Z"):
        return name
    try:
        p = subprocess.run(
            ["c++filt", "-p", name],
            capture_output=True, text=True, timeout=5,
        )
        if p.returncode == 0 and p.stdout.strip():
            return p.stdout.strip()
    except Exception:
        pass
    return name


# ---------------------------------------------------------------------------
# PE analysis (pefile)
# ---------------------------------------------------------------------------

def analyze_pe(path):
    import pefile

    pe = pefile.PE(path, fast_load=False)
    info = OrderedDict()
    info["format"] = "PE32"
    info["machine"] = hex(pe.FILE_HEADER.Machine)
    info["arch"] = {0x14C: "x86", 0x8664: "x86_64"}.get(
        pe.FILE_HEADER.Machine, "unknown")
    info["timestamp"] = pe.FILE_HEADER.TimeDateStamp
    info["characteristics"] = hex(pe.FILE_HEADER.Characteristics)
    info["dll_characteristics"] = hex(
        pe.OPTIONAL_HEADER.DllCharacteristics)
    info["image_base"] = hex(pe.OPTIONAL_HEADER.ImageBase)
    info["subsystem"] = pe.OPTIONAL_HEADER.Subsystem

    info["sections"] = [
        {"name": s.Name.rstrip(b"\x00").decode("ascii", "replace"),
         "vaddr": hex(s.VirtualAddress), "vsize": s.Misc_VirtualSize,
         "rawsize": s.SizeOfRawData}
        for s in pe.sections
    ]

    # Exports
    exports = []
    if hasattr(pe, "DIRECTORY_ENTRY_EXPORT"):
        for exp in pe.DIRECTORY_ENTRY_EXPORT.symbols:
            exports.append({
                "name": exp.name.decode("ascii", "replace") if exp.name else
                        "ordinal_%d" % exp.ordinal,
                "ordinal": exp.ordinal,
                "rva": hex(exp.forwarder or 0) if exp.forwarder else
                        hex(exp.address),
            })
    info["exports"] = exports

    # Imports grouped by DLL
    imports = OrderedDict()
    if hasattr(pe, "DIRECTORY_ENTRY_IMPORT"):
        for entry in pe.DIRECTORY_ENTRY_IMPORT:
            dll = entry.dll.decode("ascii", "replace")
            imports[dll] = sorted({
                imp.name.decode("ascii", "replace").split("@")[0]
                for imp in entry.imports if imp.name
            })
    info["imports"] = imports

    # Debug directory (CodeView pointer etc.)
    dbg = []
    if hasattr(pe, "DIRECTORY_ENTRY_DEBUG"):
        for d in pe.DIRECTORY_ENTRY_DEBUG:
            dbg.append({"type": d.struct.Type,
                        "size": d.struct.SizeOfData,
                        "rva": hex(d.struct.AddressOfRawData)})
    info["debug"] = dbg

    with open(path, "rb") as fp:
        data = fp.read()
    info["strings"] = extract_strings(data)
    return info


# ---------------------------------------------------------------------------
# ELF analysis (pyelftools)
# ---------------------------------------------------------------------------

def analyze_elf(path):
    from elftools.elf.elffile import ELFFile

    with open(path, "rb") as fp:
        pe = ELFFile(fp)
        info = OrderedDict()
        info["format"] = "ELF"
        info["elfclass"] = pe.elfclass
        info["arch"] = {
            "EM_386": "x86", "EM_X86_64": "x86_64"}.get(
            pe.get_machine_arch(), pe.get_machine_arch())
        info["type"] = pe.header["e_type"]
        info["entry"] = hex(pe.header["e_entry"])

        defined, undefined = [], []
        rtti = []
        for secname in (".dynsym", ".symtab"):
            sec = pe.get_section_by_name(secname)
            if sec is None:
                continue
            for sym in sec.iter_symbols():
                nm = sym.name
                if not nm:
                    continue
                rec = {"name": nm, "demangled": demangle_simple(nm),
                       "addr": hex(sym["st_value"]),
                       "bind": sym["st_info"]["bind"],
                       "type": sym["st_info"]["type"]}
                if sym["st_shndx"] == "SHN_UNDEF":
                    undefined.append(rec)
                else:
                    defined.append(rec)
                if nm.startswith(("_ZTI", "_ZTV", "_ZTS")):
                    rtti.append(rec)
        info["exports_defined"] = defined
        info["imports_undefined"] = undefined
        info["rtti_raw"] = rtti

        # Needed libraries
        info["needed"] = []
        dyn = pe.get_section_by_name(".dynamic")
        if dyn:
            for tag in dyn.iter_tags():
                if tag.entry.d_tag == "DT_NEEDED":
                    info["needed"].append(tag.needed)

        data = open(path, "rb").read()
        info["strings"] = extract_strings(data)
    return info


# ---------------------------------------------------------------------------
# Cross-binary blueprint extraction
# ---------------------------------------------------------------------------

HRC_RE = re.compile(r"^hrcbot_[a-z0-9_]+$")
IFACE_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_]*?\d{3}$")
THROW_RE = re.compile(r'THROW\s+(?:this==NULL\s+)?"([^"]+\.(?:cpp|h|c))"\s+(\d+)')
SRC_RE = re.compile(r"([A-Za-z0-9_\\/.-]+\\(?:Bot|Dedale|TList|TSortedList)[A-Za-z0-9_\\/.-]*\.(?:cpp|h))")
WEAPON_HINT = ("weapon_", "item_")


def rtti_classes_from_strings(strings):
    """Recover class names from both MSVC and Itanium RTTI string blobs."""
    classes = OrderedDict()
    for _off, s in strings:
        # MSVC: .?AV<name>@@ possibly with @ nesting
        m = re.match(r"^\.\?AV(.+?)@@$", s)
        if m:
            nm = m.group(1).replace("@", "::")
            classes[nm] = "msvc"
            continue
        # Itanium typeinfo names: <len><name> handled by _ZTI demangle elsewhere;
        # also catch bare length-prefixed template names like "6Dedale".
        m = re.match(r"^\d+([A-Za-z_][A-Za-z0-9_]*)$", s)
        if m and s[0].isdigit():
            classes.setdefault(m.group(1), "itanium-len")
    return classes


def parse_itanium_typeinfo_names(strings):
    """Decode Itanium nested type names such as 11TCollectionI4IBotLb0EE."""
    out = []

    def decode(s, i, depth=0):
        num = ""
        while i < len(s) and s[i].isdigit():
            num += s[i]
            i += 1
        if not num:
            return "", i
        n = int(num)
        token = s[i:i + n]
        i += n
        # template args
        while i < len(s) and s[i] == "I":
            inner, i = decode(s, i + 1, depth + 1)
            token += "<%s>" % inner
            if i < len(s) and s[i] == "E":
                i += 1
        # bool non-type params Lb0E / Lb1E
        while i < len(s) and s[i:i+2] == "Lb":
            j = s.find("E", i)
            token += "::" + s[i+2:j].replace("0", "false").replace("1", "true")
            i = j + 1
        return token, i

    for _off, s in strings:
        if re.match(r"^\d+[A-Za-z_]", s) and ("I" in s or s[0].isdigit()):
            try:
                name, _ = decode(s, 0)
                if name and name[0].isalpha():
                    out.append((s, name))
            except Exception:
                pass
    return out


def extract_source_anchors(strings):
    anchors = OrderedDict()
    for _off, s in strings:
        for m in THROW_RE.finditer(s):
            anchors.setdefault(m.group(1).replace("\\", "/"), set()).add(
                int(m.group(2)))
        m2 = SRC_RE.search(s)
        if m2:
            p = m2.group(1).replace("\\", "/").split("/")[-1]
            anchors.setdefault(p, set())
    # convert sets to sorted lists for JSON
    return {k: sorted(v) for k, v in anchors.items()}


def extract_cvars(strings):
    """Pair hrcbot_* names with the immediately following help text."""
    cvars = OrderedDict()
    rows = strings
    for idx, (_off, s) in enumerate(rows):
        name = s.strip()
        if HRC_RE.match(name) and not name.endswith(("_bot", "_core",
                                                      "_map", "_plugin")):
            help_text = ""
            # help text is usually the next non-name, non-format literal
            for _o2, t in rows[idx + 1:idx + 4]:
                t = t.strip()
                if not t:
                    continue
                if HRC_RE.match(t) or t.startswith("%") or "THROW" in t:
                    continue
                if len(t) > 6 and (" " in t or t.endswith(".")):
                    help_text = t
                    break
            cvars[name] = help_text
    return cvars


def extract_interfaces(strings):
    found = OrderedDict()
    wanted = ("VEngine", "VFile", "VPhysics", "ServerGame", "PlayerInfo",
              "EngineTrace", "GAMEEVENTS", "ISERVERPLUGIN", "BotManager",
              "StaticProp", "VClientEntity", "DataCache", "MDLCache",
              "VModelInfo", "NetworkString", "VSound", "MaterialSystem",
              "VGUI", "MapData", "COLORCORRECTION", "VSERVER", "QueuedLoader",
              "InputSystem", "NetworkSystem", "MatSystemSurface",
              "DebugTexture", "VDme", "VMDLLIB", "VStudioRender", "VAvi",
              "VBik", "VP4", "VBAlloc")
    for _off, s in strings:
        t = s.strip()
        if any(t.startswith(w) for w in wanted) or t.endswith("_VERSION_1"):
            found[t] = True
    return list(found)


def categorize_strings(strings):
    cats = {
        "chat_and_log": [],
        "weapons_and_items": [],
        "model_paths": [],
        "user_messages": [],
        "format_strings": [],
        "file_paths": [],
    }
    for _off, s in strings:
        t = s.strip()
        if not t:
            continue
        low = t.lower()
        if low.startswith(WEAPON_HINT):
            cats["weapons_and_items"].append(t)
        elif low.endswith(".mdl") or "models/" in low:
            cats["model_paths"].append(t)
        elif t.endswith((".hrcbot", ".hrcbot2", ".txt", ".vdf")) or \
                "addons/" in low:
            cats["file_paths"].append(t)
        elif "%" in t and ("%s" in t or "%i" in t or "%f" in t or
                           "%p" in t or "%d" in t):
            cats["format_strings"].append(t)
        elif re.match(r"^[A-Za-z_]+$", t) and t.isidentifier() and \
                t[0].isupper() and len(t) < 26 and " " not in t:
            cats["user_messages"].append(t)
        elif len(t) > 12 and (" " in t) and not t.startswith(("THROW",
                "HRC", "Hurricane Bot ver")):
            cats["chat_and_log"].append(t)
    for k in cats:
        cats[k] = list(OrderedDict.fromkeys(cats[k]))
    return cats


# ---------------------------------------------------------------------------
# .hrcbot container + names table
# ---------------------------------------------------------------------------

def analyze_hrcbot_file(path):
    with open(path, "rb") as fp:
        data = fp.read()
    # The container begins with a short encoded header; the first ascii run is
    # the version banner and the second is the map name.
    ascii_runs = []
    cur = bytearray()
    start = 0
    for i, b in enumerate(data[:256]):
        if 0x20 <= b < 0x7F:
            if not cur:
                start = i
            cur.append(b)
        else:
            if len(cur) >= 4:
                ascii_runs.append((start, cur.decode("ascii")))
            cur = bytearray()
    return {
        "file": os.path.basename(path),
        "size": len(data),
        "leading_bytes": data[:8].hex(),
        "ascii_header_runs": ascii_runs[:4],
    }


def analyze_names_file(path):
    with open(path, "r", encoding="latin-1") as fp:
        names = [ln.strip() for ln in fp if ln.strip()]
    return {"file": os.path.basename(path), "count": len(names),
            "names": names}


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------

def build_blueprint(pkg):
    addons = os.path.join(pkg, "addons")
    dll = os.path.join(addons, "hrcbot_server_plugin.dll")
    so = os.path.join(addons, "hrcbot_server_plugin.so")

    pe = analyze_pe(dll) if os.path.exists(dll) else None
    elf = analyze_elf(so) if os.path.exists(so) else None

    # Merge evidence, preferring whichever binary retained more strings.
    def strings_of(b):
        return b["strings"] if b else []

    pe_str = strings_of(pe)
    elf_str = strings_of(elf)
    merged_str = pe_str + [s for s in elf_str if s not in set(pe_str)]

    blueprint = OrderedDict()
    blueprint["product"] = "Hurricane Bot (HRCBot) server plugin"
    blueprint["package"] = os.path.basename(os.path.normpath(pkg))

    # Version banner (very reliable, taken from log strings)
    banners = sorted({s for _o, s in elf_str
                      if "Hurricane Bot ver" in s})
    blueprint["version_banners"] = banners

    blueprint["binaries"] = {"windows_pe": _summary(pe),
                             "linux_elf": _summary(elf)}

    blueprint["engine_interfaces"] = extract_interfaces(merged_str)
    blueprint["cvars_and_commands"] = extract_cvars(merged_str)
    blueprint["rtti_classes"] = sorted(
        set(rtti_classes_from_strings(pe_str)) |
        {n for _raw, n in parse_itanium_typeinfo_names(elf_str)})
    blueprint["itanium_templates"] = sorted({
        n for _r, n in parse_itanium_typeinfo_names(elf_str)})
    blueprint["source_anchors"] = extract_source_anchors(merged_str)
    blueprint["string_categories"] = categorize_strings(merged_str)

    # imports surface
    if pe:
        blueprint["engine_imports_windows"] = pe["imports"]
    if elf:
        blueprint["engine_imports_linux"] = sorted({
            r["demangled"].split("@")[0]
            for r in elf["imports_undefined"]
            if not r["demangled"].startswith(("_", "__")) or
            r["demangled"].startswith("_Z")})
        blueprint["elf_needed"] = elf["needed"]
        blueprint["elf_exports"] = [
            r["name"] for r in elf["exports_defined"]
            if r["type"] == "FUNC" and r["bind"] == "GLOBAL"]

    # data files
    plugdir = os.path.join(addons, "hrcbot_server_plugin")
    hrc_files, names = [], None
    if os.path.isdir(plugdir):
        for f in sorted(os.listdir(plugdir)):
            p = os.path.join(plugdir, f)
            if f.endswith(".hrcbot"):
                hrc_files.append(analyze_hrcbot_file(p))
            elif f.endswith(".txt"):
                names = analyze_names_file(p)
    blueprint["waypoint_files"] = hrc_files
    blueprint["bot_names"] = names

    # keep raw ordered strings, but out of the JSON (written separately)
    return blueprint, pe_str, elf_str


def _summary(b):
    if not b:
        return None
    keep = {k: v for k, v in b.items() if k not in ("strings",)}
    return keep


def write_markdown(bp, path):
    L = []
    L.append("# HRCBot recovered blueprint\n")
    L.append("Product: **%s**\n" % bp["product"])
    for b in bp.get("version_banners", []):
        L.append("- Build banner: `%s`" % b)
    L.append("")

    L.append("## Engine interface versions\n")
    for i in bp["engine_interfaces"]:
        L.append("- `%s`" % i)
    L.append("")

    L.append("## ConVars and ConCommands\n")
    L.append("| Name | Help / description (recovered, may be partial) |")
    L.append("| --- | --- |")
    for k, v in bp["cvars_and_commands"].items():
        L.append("| `%s` | %s |" % (k, v.replace("|", "\\|")))
    L.append("")

    L.append("## Recovered classes (RTTI)\n")
    for c in bp["rtti_classes"]:
        L.append("- `%s`" % c)
    L.append("")

    L.append("## Source file anchors (assert strings)\n")
    for f, lines in sorted(bp["source_anchors"].items()):
        L.append("- `%s` (assert lines: %s)" %
                 (f, ", ".join(map(str, lines)) if lines else "n/a"))
    L.append("")

    sc = bp["string_categories"]
    for title, key in (("Weapons / items", "weapons_and_items"),
                       ("Model paths", "model_paths"),
                       ("File paths", "file_paths"),
                       ("User/net message candidates", "user_messages")):
        L.append("## %s\n" % title)
        for s in sc[key][:120]:
            L.append("- `%s`" % s)
        L.append("")

    if bp.get("waypoint_files"):
        L.append("## Waypoint (.hrcbot) containers\n")
        for w in bp["waypoint_files"]:
            hdr = "; ".join(r[1] for r in w["ascii_header_runs"][:2])
            L.append("- `%s` (%d bytes) header: %s" %
                     (w["file"], w["size"], hdr))
        L.append("")
    if bp.get("bot_names"):
        L.append("## Bot names (%d)\n" % bp["bot_names"]["count"])
        L.append(", ".join(bp["bot_names"]["names"]))
        L.append("")
    with open(path, "w", encoding="utf-8") as fp:
        fp.write("\n".join(L))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pkg", required=True,
                    help="extracted hrcbot-1.3.4-* package directory")
    ap.add_argument("--out", default="analysis")
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    bp, pe_str, elf_str = build_blueprint(args.pkg)

    with open(os.path.join(args.out, "blueprint.json"), "w",
              encoding="utf-8") as fp:
        json.dump(bp, fp, indent=2, ensure_ascii=False)
    write_markdown(bp, os.path.join(args.out, "BLUEPRINT.md"))

    with open(os.path.join(args.out, "strings_windows_pe.txt"), "w",
              encoding="utf-8") as fp:
        fp.write("\n".join("%08x  %s" % (o, s) for o, s in pe_str))
    with open(os.path.join(args.out, "strings_linux_elf.txt"), "w",
              encoding="utf-8") as fp:
        fp.write("\n".join("%08x  %s" % (o, s) for o, s in elf_str))

    print("Blueprint written to", args.out)
    print("  cvars/commands :", len(bp["cvars_and_commands"]))
    print("  interfaces     :", len(bp["engine_interfaces"]))
    print("  rtti classes   :", len(bp["rtti_classes"]))
    print("  source anchors :", len(bp["source_anchors"]))


if __name__ == "__main__":
    main()
