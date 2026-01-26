from os.path import dirname, realpath, basename, isfile, abspath, isdir
import sys
import importlib.util
from os import getenv, environ

# root/site_scons/sbt/loader.py -> root/site_scons
__sbt_root_path = dirname(dirname(realpath(__file__)))

# root/site_scons/sbt/loader.py -> root
__project_root_path = dirname(__sbt_root_path)

sys.path.append(__sbt_root_path)
sys.path.append(__project_root_path)

from sbt import logger
from site_scons.sbt.build import utils

def __import_module_no_bytecode(module_name):
    """
    Import a module without writing bytecode files
    """
    before = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        return __import__(module_name, fromlist=[''])
    except ImportError as e:
        logger.error(f"Failed to import module {module_name}: {e}")
        raise
    finally:
        sys.dont_write_bytecode = before

def __import_from_path(file_path):
    """
    Import a module from a file path without writing bytecode files.
    Supports both absolute and relative paths, including subdirectories.
    """
    before = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        # If path is relative, resolve it relative to project root
        if not file_path.startswith('/'):
            file_path = abspath(__project_root_path + '/' + file_path)
        else:
            file_path = abspath(file_path)
        if not isfile(file_path):
            raise ImportError(f"File not found: {file_path}")
        
        module_name = basename(file_path).replace(".py", "")
        spec = importlib.util.spec_from_file_location(module_name, file_path)
        if spec is None or spec.loader is None:
            raise ImportError(f"Could not load module spec from: {file_path}")
        
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    finally:
        sys.dont_write_bytecode = before

def load_env():
    env_module = None
    env_module_name = "env.py"

    env_arg = utils.get_opt("env", None)
    if env_arg is not None:
        logger.debug(f"Loading specific environment from: {env_arg}")
        env_module_name = env_arg

    try:
        env_module = __import_from_path(env_module_name)
    except ImportError as e:
        logger.debug(f"Could not load env from path {env_module_name}: {e}")
        pass

    if env_module is not None:
        env_keys = [item for item in dir(env_module) if not item.startswith("__")]
        for env_key in env_keys:
            if getenv(env_key, "") == "":
                value = getattr(env_module, env_key)
                environ[env_key] = str(value)

    return env_module

def load_app():
    try:
        return __import_module_no_bytecode("app")
    except ImportError as e:
        logger.error(f"Failed to load app module: {e}")
        raise