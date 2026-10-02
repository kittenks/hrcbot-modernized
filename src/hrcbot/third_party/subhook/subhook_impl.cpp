// subhook_impl.cpp
//
// Translation unit that compiles the vendored subhook inline-hooking
// library (BSD 2-clause, Copyright (c) 2012-2018 Zeex) together with the
// plugin.  subhook is written in C; we build it through this C++ wrapper so
// it is picked up by the project's single C++ compiler on every platform.
//
// SUBHOOK_STATIC keeps the symbols private to the plugin binary (no
// dllexport/dllimport) on Windows.  Two small C++ compatibility fixes are
// applied to the vendored sources (explicit calloc() cast and __cplusplus
// guards around the true/false macros).
#ifndef SUBHOOK_STATIC
#define SUBHOOK_STATIC
#endif

#include "subhook.c"
