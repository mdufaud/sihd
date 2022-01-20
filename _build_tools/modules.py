import os

expected_configurations_lists = ['depends', 'libs', 'parse-configs', 'pkg-configs']

def fill_modlist_from_modules(modules, specific_modules, modlist):
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

def __rec_fill_module_real_depends(modules, module_name, to_fill_module_conf):
    """ Fill a single module real dependency tree into modconf """
    conf = modules.get(module_name, None)
    if conf is None:
        raise RuntimeError("Error in module's configuration, not a module: {}".format(module_name))
    depends = conf.get('depends', [])
    for expected_conf in expected_configurations_lists:
        to_fill_module_conf[expected_conf].extend(conf.get(expected_conf, []))
    for module in depends:
        __rec_fill_module_real_depends(modules, module, to_fill_module_conf)

def resolve_modules_dependencies(modules):
    """ Fill all modules real dependency tree """
    for name, conf in modules.items():
        conf["modname"] = name
        # Add configurations if not declared
        for expected_conf in expected_configurations_lists:
            if expected_conf not in conf:
                conf[expected_conf] = []
        # Adds conditionnal dependencies if they are in the current build
        conditionnal_depends = conf.get("conditionnal-depends", [])
        if conditionnal_depends:
            conf['depends'].extend([cond_mod for cond_mod in conditionnal_depends if cond_mod in modules])
        # Get dependency tree
        __rec_fill_module_real_depends(modules, name, conf)
        # Remove duplicates
        for expected_conf in expected_configurations_lists:
            conf[expected_conf] = list(set(conf[expected_conf]))

def get_global_extlibs(app):
    libs = hasattr(app, "libs") and app.libs or []
    return libs

def get_extlibs_versions(app, modules_extlibs):
    if not hasattr(app, "extlibs"):
        return {}
    extlibs = app.extlibs
    ret = {}
    # matching with extlibs
    for extlib in modules_extlibs:
        for libname, version in extlibs.items():
            if libname == extlib:
                ret[libname] = version
    return ret

def get_modules_extlibs(app, modules):
    """ Gets all libs versions needed by selected modules
        @return dict[libname] = version
    """
    modules_extlibs = set()
    # getting used libs for every modules
    for _, module in modules.items():
        libs = module.get('use-extlibs', [])
        for lib in libs:
            modules_extlibs.add(lib)
    # adding global libs + test_libs
    for extlib in get_global_extlibs(app):
        modules_extlibs.add(extlib)
    return get_extlibs_versions(app, modules_extlibs)

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

def remove_module(app, modname):
    if hasattr(app, "modules") and modname in app.modules:
        del app.modules[modname]
    if hasattr(app, "conditionnal_modules") and modname in app.conditionnal_modules:
        del app.conditionnal_modules[modname]

def get_module_merged_with_conditionnals(app):
    ret = {}
    if hasattr(app, "modules"):
        for key, value in app.modules.items():
            ret[key] = value
    if hasattr(app, "conditionnal_modules"):
        for key, value in app.conditionnal_modules.items():
            ret[key] = value
    return ret

def get_build_order(modules):
    order = []
    keys = modules.keys()
    lenkeys = len(keys)
    while len(order) != lenkeys:
        for modname, conf in modules.items():
            if modname in order:
                continue
            depends = conf.get('depends', None)
            if depends is not None:
                depends = conf['depends']
                # check if all dependencies are in keys and check if all dependencies already are in order
                if all(depend in keys for depend in depends) \
                    and all(depend in order for depend in depends):
                    order.append(modname)
            else:
                order.append(modname)
    return order

def get_conditionnals_from_env(app):
    ret = []
    for key, conf in app.conditionnal_modules.items():
        cond_env = conf.get("conditionnal-env", None)
        if cond_env is not None and os.getenv(cond_env, None):
            ret.append(key)
    return ret

def get_modules(app, specific_modules="", conditionnals=[]):
    """ @brief build modules from application configuration
        @param app the application configuration module
        @param specific_modules build specific comma separated modules instead of all
        @param conditionnals list of conditionnal modules to be added to the build
        @return list
    """
    if not hasattr(app, "modules"):
        raise RuntimeError("App's configuration file should have modules")
    conditionnals.extend(get_conditionnals_from_env(app))
    modules = {}
    specific_modules_list = specific_modules.split(',')
    if specific_modules:
        app_modules = app.modules
        # if a conditionnal is in specific module list, add conditionnals to list of modules
        if has_conditionnal_in_modules_list(app.conditionnal_modules, specific_modules_list):
            app_modules = get_module_merged_with_conditionnals(app)
        fill_modlist_from_modules(app_modules, specific_modules_list, modules)
    else:
        modules = app.modules
    # Add specific conditionnal modules
    for modname in conditionnals:
        add_conditionnal_module(app.conditionnal_modules, modules, modname)
    # Get every dependencies configuration for modules
    resolve_modules_dependencies(modules)
    return modules