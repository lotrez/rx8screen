Import("env")
import shutil, os

build_dir = env.subst("$BUILD_DIR")
dll_src = os.path.join("sdl2", "SDL2-2.32.8", "lib", "x64", "SDL2.dll")
dll_dst = os.path.join(build_dir, "SDL2.dll")

def copy_dll(source, target, env):
    shutil.copy2(dll_src, dll_dst)

env.AddPostAction("$BUILD_DIR/${PROGNAME}.exe", copy_dll)
