from sbt import builder
from sbt import logger
import os

def load_in_env(env):
    env.Replace(
        CC = "emcc",
        CXX = "em++",
        AR = "emar",
        RANLIB = "emranlib",
        LINK = "emcc",
    )

    emscripten_conf = os.path.join(os.getenv("HOME"), ".emscripten")
    if os.path.isfile(emscripten_conf):
        try:
            exec(open(emscripten_conf).read())
        except Exception as e:
            logger.warning("could not execute emscripten configuration: " + emscripten_conf)