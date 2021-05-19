def fill_modlist_from_single_module(modules, module_name, modlist: dict):
    """ Gets all modules to build from a single module to build """
    conf = modules.get(module_name, None)
    if conf is None:
        raise RuntimeError("No such module: {}".format(module_name))
    modlist[module_name] = conf
    for dep in conf.get('depends', []):
        fill_modlist_from_single_module(modules, dep, modlist)

def __rec_fill_module_real_depends(modules, module_name, modconf: dict):
    """ Fill a single module real dependency tree into modconf """
    conf = modules.get(module_name, None)
    if conf is None:
        raise RuntimeError("No such module: {}".format(module_name))
    depends = conf.get('depends', [])
    modconf['depends'] += depends
    modconf['libs'] += conf.get('libs', [])
    modconf['headers'] += conf.get('headers', [])
    modconf['parse-configs'] += conf.get('parse-configs', [])
    modconf['pkg-configs'] += conf.get('pkg-configs', [])
    for depend in depends:
        __rec_fill_module_real_depends(modules, depend, modconf)

def fill_all_modules_dependencies(modules: dict):
    """ Fill all modules real dependency tree """
    for name, conf in modules.items():
        # Add configurations if not declared
        if 'depends' not in conf:
            conf['depends'] = []
        if 'libs' not in conf:
            conf['libs'] = []
        if 'headers' not in conf:
            conf['headers'] = []
        if 'parse-configs' not in conf:
            conf['parse-configs'] = []
        if 'pkg-configs' not in conf:
            conf['pkg-configs'] = []
        # Get dependency tree
        __rec_fill_module_real_depends(modules, name, conf)
        # Remove duplicates
        conf['depends'] = list(set(conf['depends']))
        conf['libs'] = list(set(conf['libs']))
        conf['headers'] = list(set(conf['headers']))
        conf['parse-configs'] = list(set(conf['parse-configs']))
        conf['pkg-configs'] = list(set(conf['pkg-configs']))

def build_libs(app, test=False):
    libs = hasattr(app, "libs") and app.libs or []
    if test and hasattr(app, "test_libs"):
        libs += app.test_libs
    return libs

def build_headers(app, test=False):
    headers = hasattr(app, "headers") and app.headers or []
    if test and hasattr(app, "test_headers"):
        headers += app.test_headers
    return headers

def build_libs_versions(app: dict, modules: dict, test=False):
    """ Gets all libs versions needed by selected modules """
    if not hasattr(app, "libs_versions"):
        return {}
    libs_versions = app.libs_versions
    modules_extlibs = set()
    for _, module in modules.items():
        libs = module.get('libs', [])
        for lib in libs:
            modules_extlibs.add(lib)
        headers = module.get('headers', [])
        for header in headers:
            modules_extlibs.add(header)
    for extlib in build_libs(app, test=test):
        modules_extlibs.add(extlib)
    ret = {}
    for extlib in modules_extlibs:
        for libname, version in libs_versions.items():
            if libname.find(extlib) >= 0:
                ret[libname] = version
    return ret

def build_modules(app, single_module="", test=False):
    if not hasattr(app, "modules"):
        raise RuntimeError("App's configuration file should have modules")
    modules = {}
    if single_module:
        fill_modlist_from_single_module(app.modules, single_module, modules)
    else:
        modules = app.modules
    fill_all_modules_dependencies(modules)
    return modules