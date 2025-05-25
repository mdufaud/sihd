from os.path import dirname, realpath, basename
import sys
from os import getenv, environ

# root/site_scons/sbt/loader.py -> root/site_scons
__sbt_root_path = dirname(dirname(realpath(__file__)))

# root/site_scons/sbt/loader.py -> root
__project_root_path = dirname(__sbt_root_path)

sys.path.append(__sbt_root_path)
sys.path.append(__project_root_path)

from sbt import logger

def __import_no_bytecode(module_name):
    """
    Import a module without writing bytecode files.
    This is useful for environments where bytecode generation is not desired.
    """
    sys.dont_write_bytecode = True
    try:
        return __import__(module_name, fromlist=[''])
    except ImportError as e:
        logger.error(f"Failed to import module {module_name}: {e}")
        raise
    finally:
        sys.dont_write_bytecode = False

def load_env():
    env_module = None
    env_module_name = "default_env"

    # Check if 'env=' is present in sys.argv
    env_arg = next((arg for arg in sys.argv if arg.startswith("env=")), None)
    if env_arg:
        env_path = env_arg.split("=", 1)[1]
        logger.debug(f"Loading environment from: {env_path}")
        env_module_name = basename(env_path).replace(".py", "")

    logger.debug(f"Loading environment module: {env_module_name}")

    try:
        env_module = __import_no_bytecode(env_module_name)
    except ImportError as e:
        logger.debug(f"No environment module found: {e}")

    if env_module is not None:
        env_keys = [item for item in dir(env_module) if not item.startswith("__")]
        for env_key in env_keys:
            if getenv(env_key, "") == "":
                value = getattr(env_module, env_key)
                environ[env_key] = str(value)

    return env_module

def load_app():
    try:
        return __import_no_bytecode("app")
    except ImportError as e:
        logger.error(f"Failed to load app module: {e}")
        raise