#!/usr/bin/env python3
# AMBuild configure entry point for the modernized HRCBot (Hurricane Bot).
#
# This project targets Half-Life 2: Deathmatch (hl2dm) only and is built as a
# Metamod:Source 1.12 plugin.  Both 32 and 64 bit Linux and Windows are
# supported through AMBuild 2.1.

import sys
import os

try:
    from ambuild2 import run, util
except ImportError:
    sys.stderr.write(
        "AMBuild 2 must be installed to build HRCBot.\n"
        "Install it with:  pip install ambuild\n")
    sys.exit(1)


def make_objdir_name(p):
    arch = p.target_arch.replace("x86_64", "x64")
    return "obj-{0}-{1}".format(util.Platform(), arch)


parser = run.BuildParser(sourcePath=sys.path[0], api="2.1")
parser.default_arch = "x86,x64"
parser.default_build_folder = make_objdir_name

parser.options.add_option("--hl2sdk-root", type=str, dest="hl2sdk_root",
                          default=os.environ.get("HL2SDKROOT"),
                          help="Root folder that contains hl2sdk-hl2dm")
parser.options.add_option("--hl2sdk-hl2dm", type=str, dest="hl2sdk_hl2dm",
                          default=os.environ.get("HL2SDKHL2DM"),
                          help="Explicit path to the hl2sdk hl2dm branch")
parser.options.add_option("--mms-path", type=str, dest="mms_path",
                          default=os.environ.get("MMSOURCE112") or
                          os.environ.get("MMSOURCE_DEV"),
                          help="Path to the Metamod:Source 1.12 source tree")
parser.options.add_option("--enable-debug", action="store_const", const="1",
                          dest="debug", help="Build with debug symbols")
parser.options.add_option("--enable-optimize", action="store_const", const="1",
                          dest="opt", default="1",
                          help="Build with optimizations (default)")
parser.options.add_option("--no-opt", action="store_const", const="0",
                          dest="opt", help="Disable optimizations")
parser.options.add_option("-s", "--sdks", default="hl2dm", dest="sdks",
                          help="SDKs to build (only hl2dm is supported)")

# The architecture list is handled by AMBuild through --target-arch
# (comma separated, e.g. --target-arch x86,x64); default_arch above selects
# both 32-bit and 64-bit when the flag is omitted.

parser.Configure()
