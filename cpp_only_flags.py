from SCons.Script import Import
Import("env")

# C++-only switches / defines:
env.Append(CXXFLAGS=[
    "-fno-exceptions",         
     "-fno-rtti",            
])

# If you ever want C-only flags, use:
# env.Append(CFLAGS=["-DMY_C_ONLY_OPT=1"])