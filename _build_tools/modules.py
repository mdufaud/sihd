def fill_modlist_from_single_module(modules, module_name, modlist: dict):
    """ Gets a single module dependency tree """
    conf = modules.get(module_name, None)
    if conf is None:
        raise RuntimeError("No such module: {}".format(module_name))
    modlist[module_name] = conf
    for dep in conf.get('depends', []):
        fill_modlist_from_single_module(modules, dep, modlist)

def __rec_fill_module_real_depends(modules, module_name, modconf: dict):
    """ Makes a map [modulename] = trashvalue
        to have a single module real dependencies """
    conf = modules.get(module_name, None)
    if conf is None:
        raise RuntimeError("No such module: {}".format(module_name))
    depends = conf.get('depends', [])
    modconf['depends'] += depends
    modconf['libs'] += conf.get('libs', [])
    modconf['headers'] += conf.get('headers', [])
    for depend in depends:
        __rec_fill_module_real_depends(modules, depend, modconf)

def fill_all_modules_dependencies(modules: dict):
    """ Fill all modules real dependency tree """
    for name, conf in modules.items():
        if 'depends' not in conf:
            conf['depends'] = []
        if 'libs' not in conf:
            conf['libs'] = []
        if 'headers' not in conf:
            conf['headers'] = []
        __rec_fill_module_real_depends(modules, name, conf)
        conf['depends'] = list(set(conf['depends']))
        conf['libs'] = list(set(conf['libs']))
        conf['headers'] = list(set(conf['headers']))

def build_libs(app, test=False):
    libs = app.libs
    if test:
        libs += app.test_libs
    return libs

def build_headers(app, test=False):
    headers = app.headers
    if test:
        headers += app.test_headers
    return headers

def build_libs_versions(app: dict, modules: dict, test=False):
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
    ret = []
    for extlib in modules_extlibs:
        for libname, version in libs_versions.items():
            if libname.find(extlib) >= 0:
                ret.append("{}/{}".format(libname, version))
    return ret

def build_modules(app, single_module="", test=False):
    modules = {}
    if single_module:
        fill_modlist_from_single_module(app.modules, single_module, modules)
    else:
        modules = app.modules
    fill_all_modules_dependencies(modules)
    return modules