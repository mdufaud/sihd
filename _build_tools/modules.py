def fill_modlist_from_modules(modules, specific_modules, modlist: dict):
    """ Gets all modules to build from a single module to build """
    if not specific_modules:
        return
    for module_name in specific_modules:
        conf = modules.get(module_name, None)
        if conf is None:
            raise RuntimeError("No such module: {}".format(module_name))
        modlist[module_name] = conf
        fill_modlist_from_modules(modules, conf.get('depends', []), modlist)
        #fill_modlist_from_modules(modules, conf.get('conditionnal-depends', []), modlist)

def __rec_fill_module_real_depends(modules, module_name, to_fill_module_conf: dict):
    """ Fill a single module real dependency tree into modconf """
    conf = modules.get(module_name, None)
    if conf is None:
        raise RuntimeError("Error in module's configuration, not a module: {}".format(module_name))
    depends = conf.get('depends', [])
    to_fill_module_conf['depends'] += depends
    to_fill_module_conf['libs'] += conf.get('libs', [])
    to_fill_module_conf['headers'] += conf.get('headers', [])
    to_fill_module_conf['parse-configs'] += conf.get('parse-configs', [])
    to_fill_module_conf['pkg-configs'] += conf.get('pkg-configs', [])
    for module in depends:
        __rec_fill_module_real_depends(modules, module, to_fill_module_conf)

def resolve_modules_dependencies(modules: dict):
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
        # Adds conditionnal dependencies if they are in the current build
        conditionnal_depends = conf.get("conditionnal-depends", [])
        if conditionnal_depends:
            conf['depends'] += [cond_mod for cond_mod in conditionnal_depends if cond_mod in modules]
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
    """ Gets all libs versions needed by selected modules
        @return dict[libname] = version
    """
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

def add_conditionnal_module(conditionnal_modules, modules, modname):
    conditionnal_module = conditionnal_modules.get(modname, None)
    if conditionnal_module is None:
        raise RuntimeError("App does not have conditionnal module named: " + modname)
    modules[modname] = conditionnal_module

def has_conditionnal_in_modules_list(app_conditionnal_modules, modules):
    ret = False
    for modname in modules:
        if modname in app_conditionnal_modules:
            ret = True
            break
    return ret
 
def check_conditionnal_modules(app):
    has_conditionnals = hasattr(app, "conditionnal_modules")
    if has_conditionnals is True:
        for modname, _ in app.conditionnal_modules.items():
            if modname in app.modules:
                raise RuntimeError("App's conditionnal module share a name with a module: " + modname)
    return has_conditionnals

def build_conditionnal_modules(app):
    ret = {}
    for key, value in app.modules.items():
        ret[key] = value
    for key, value in app.conditionnal_modules.items():
        ret[key] = value
    return ret

def build_modules(app, specific_modules="", conditionnals=[]):
    """ @brief build modules from application configuration
        @param app the application configuration module
        @param specific_modules build specific comma separated modules instead of all
        @param conditionnals list of conditionnal modules to be added to the build
        @return list
    """
    if not hasattr(app, "modules"):
        raise RuntimeError("App's configuration file should have modules")
    has_conditionnals = check_conditionnal_modules(app)
    if conditionnals and not has_conditionnals:
        raise RuntimeError("App's configuration does not have conditionnal modules")
    modules = {}
    specific_modules_list = specific_modules.split(',')
    if specific_modules:
        app_modules = app.modules
        if has_conditionnals and has_conditionnal_in_modules_list(app.conditionnal_modules, specific_modules_list):
            app_modules = build_conditionnal_modules(app)
        fill_modlist_from_modules(app_modules, specific_modules_list, modules)
    else:
        modules = app.modules
    # Add specific conditionnal modules
    for modname in conditionnals:
        add_conditionnal_module(app.conditionnal_modules, modules, modname)
    # Get every dependencies configuration for modules
    resolve_modules_dependencies(modules)
    return modules