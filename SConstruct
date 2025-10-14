#!/usr/bin/env python
import os
import sys

GODOT_BINDINGS = os.getenv("GODOT_BINDINGS", "godot-cpp/SConstruct")
GODOT_BINDINGS = ARGUMENTS.get('GODOT_BINDINGS', GODOT_BINDINGS)
env = SConscript(GODOT_BINDINGS + "/SConstruct")

# For reference:
# - CCFLAGS are compilation flags shared between C and C++
# - CFLAGS are for C-specific compilation flags
# - CXXFLAGS are for C++-specific compilation flags
# - CPPFLAGS are for pre-processor flags
# - CPPDEFINES are for pre-processor defines
# - LINKFLAGS are for linking flags

# tweak this if you want to use different folders, or more folders, to store your source code in.
env.Append(CPPPATH=["./"])
env.Append(CPPDEFINES=["TERRAINER_GDEXTENSION"])
sources = Glob("./*.cpp")

if env["target"] == "editor":
    sources.append(Glob("./editor/*.cpp"))

BIN_OUTPUT = os.getenv("BIN_OUTPUT", "./demo/bin")
BIN_OUTPUT = ARGUMENTS.get('BIN_OUTPUT', BIN_OUTPUT)

if env["platform"] == "macos":
    library = env.SharedLibrary(
        BIN_OUTPUT + "/libterrainer.{}.{}.framework/libterrainer.{}.{}".format(
            env["platform"], env["target"], env["platform"], env["target"]
        ),
        source=sources,
    )
elif env["platform"] == "ios":
    if env["ios_simulator"]:
        library = env.StaticLibrary(
            BIN_OUTPUT + "/libterrainer.{}.{}.simulator.a".format(env["platform"], env["target"]),
            source=sources,
        )
    else:
        library = env.StaticLibrary(
            BIN_OUTPUT + "/libterrainer.{}.{}.a".format(env["platform"], env["target"]),
            source=sources,
        )
else:
    library = env.SharedLibrary(
        BIN_OUTPUT + "/libterrainer{}{}".format(env["suffix"], env["SHLIBSUFFIX"]),
        source=sources,
    )

Default(library)
