import os
import sys

must_have_parameters = ['depends', 'libs', 'link', 'flags']

def fill_modlist_from_modules(modules, specific_modules, modlist):
    """ @brief Gets all modules to build from a single module to build
    """
    if not specific_modules:
        return
    for module_name in specific_modules:
        conf = modules.get(module_name, None)
        if conf is None:
            raise RuntimeError("No such module: {}".format(module_name))
        modlist[module_name] = conf
        fill_modlist_from_modules(modules, conf.get('depends', []), modlist)
        #fill_modlist_from_modules(modules, conf.get('conditional-depends', []), modlist)

def __rec_fill_module_real_depends(modules, module_name, to_fill_module_conf):
    """ @brief Fill a single module real dependency tree into modules
    """
    conf = modules.get(module_name, None)
    if conf is None:
        raise RuntimeError("Error in module's configuration, not a module: {}".format(module_name))
    curr_depends = to_fill_module_conf.setdefault("depends", [])
    depends = conf.get('depends', [])
    for dependancy in depends:
        if dependancy not in curr_depends:
            curr_depends.insert(0, dependancy)
    for depending_module_name in depends:
        __rec_fill_module_real_depends(modules, depending_module_name, to_fill_module_conf)

def __get_module_fill_order(modules):
    order = []
    iterations = 0
    # maximum is worst case scenario
    max_iterations = pow(len(modules), 2)
    while len(order) != len(modules):
        for modname, conf in modules.items():
            if modname in order:
                continue
            depends = conf.get("depends", [])
            if all(dep in order for dep in depends):
                order.append(modname)
        iterations += 1
        if iterations > max_iterations:
            print("module order error", file = sys.stderr)
            print("maximum module order completion: {}".format(order), file = sys.stderr)
            for modname, conf in modules.items():
                print("module[{}] dependancies -> {}".format(modname, conf.get("depends", [])), file = sys.stderr)
            raise SystemExit("maximum iterations reached to get modules build order - check your app configuration")
    return order

def resolve_modules_dependencies(modules):
    """ @brief Fill all modules real dependency tree
        @param modules modules dict
    """
    order = __get_module_fill_order(modules)
    for name in order:
        conf = modules[name]
        conf["modname"] = name
        # Adds must have parameters
        for param in must_have_parameters:
            if param not in conf:
                conf[param] = []
        conf["original-depends"] = conf["depends"]
        # Adds conditional dependencies if they are in the current build
        conditional_depends = conf.get("conditional-depends", [])
        if conditional_depends:
            conf['depends'].extend([cond_mod for cond_mod in conditional_depends if cond_mod in modules])
        # Get dependency tree
        __rec_fill_module_real_depends(modules, name, conf)

def get_module_libs(modules, modname, add_depends_libs = False):
    conf = modules[modname]
    libs = []
    if add_depends_libs:
        for dep in conf["depends"]:
            dep_conf = modules[dep]
            dep_libs = dep_conf.get('libs', [])
            libs[:0] = dep_libs
    libs[:0] = conf['libs']
    return libs

def get_extlibs_versions(app, modules_extlibs):
    extlibs = getattr(app, "extlibs", {})
    ret = {}
    # matching with extlibs
    for extlibname in modules_extlibs:
        for libname, version in extlibs.items():
            if libname == extlibname:
                ret[libname] = version
    return ret

def get_modules_extlibs(app, modules, platform):
    """ Gets all libs versions needed by selected modules
        @param app the application configuration module
        @return dict as dict[libname] = version
    """
    modules_extlibs = set()
    # getting used libs for every modules
    for _, module in modules.items():
        modules_extlibs.update(module.get('extlibs', []))
        modules_extlibs.update(module.get('{}-extlibs'.format(platform), []))
    return get_extlibs_versions(app, modules_extlibs)

def get_modules_packages(app, packet_manager_name, modules_extlibs):
    pkg_manager_conf_name = "{}_packages".format(packet_manager_name)
    pkg_manager_conf = getattr(app, pkg_manager_conf_name, None)
    if pkg_manager_conf is None:
        raise RuntimeError("app configuration does not have a package manager conf named: '{}'".format(pkg_manager_conf_name))
    ret = {}
    missing = []
    for libname, version in modules_extlibs.items():
        package_libname = pkg_manager_conf.get(libname, None)
        if package_libname is None:
            missing.append(libname)
            continue
        ret[package_libname] = version
    return ret, missing

def add_conditional_module(conditional_modules, modules, modname):
    """ @brief check if a conditional module is in modules list
        @param conditional_modules modules dictionnary from application's conf
        @param modules modules dict to add to
        @param modname the module to be added from conditional_modules
    """
    conditional_module = conditional_modules.get(modname, None)
    if conditional_module is None:
        raise RuntimeError("App does not have conditional module named: " + modname)
    modules[modname] = conditional_module

def has_conditional_in_modules_list(conditional_modules, modules):
    """ @brief check if a conditional module is in modules list
        @param conditional_modules modules dictionnary from application's conf
        @param modules modules dict to check
        @return bool
    """
    ret = False
    for modname in modules:
        if modname in conditional_modules:
            ret = True
            break
    return ret

def check_conditional_modules(app):
    """ @brief check if application conf has conditional modules and check if names are unique
        @param app the application configuration module
        @return bool
    """
    has_conditionals = hasattr(app, "conditional_modules")
    if has_conditionals is True:
        for modname, _ in app.conditional_modules.items():
            if modname in app.modules:
                raise RuntimeError("App's conditional module share a name with a module: " + modname)
    return has_conditionals

def remove_module(app, modname):
    """ @brief removes a module from application conf
        @param app the application configuration module
        @param modname the module to be removed
    """
    if hasattr(app, "modules") and modname in app.modules:
        del app.modules[modname]
    if hasattr(app, "conditional_modules") and modname in app.conditional_modules:
        del app.conditional_modules[modname]

def get_module_merged_with_conditionals(app):
    """ @brief merge app.modules and app.conditional_modules
        @param app the application configuration module
        @return dict of complete modules
    """
    ret = {}
    if hasattr(app, "modules"):
        for key, value in app.modules.items():
            ret[key] = value
    if hasattr(app, "conditional_modules"):
        for key, value in app.conditional_modules.items():
            ret[key] = value
    return ret

def get_conditionals_from_env(app):
    """ @brief gets conditional modules based on their presence in env
        @param app the application configuration module
        @return list of module names
    """
    ret = []
    if hasattr(app, "conditional_modules"):
        for key, conf in app.conditional_modules.items():
            cond_env = conf.get("conditional-env", None)
            if cond_env is not None and os.getenv(cond_env, None) == "1":
                ret.append(key)
    return ret

def check_platform(modules, platform):
    """ @brief sanatize modules based on platform
        @return modules to remove
    """
    to_remove = []
    for name, conf in modules.items():
        module_platforms = conf.get("platforms", None)
        if module_platforms is not None and platform not in module_platforms:
            to_remove.append(name)
    for name in to_remove:
        del modules[name]
    return to_remove

def build_modules_conf(app, specific_modules=[], conditionals=[]):
    """ @brief build modules from application configuration
        @param app the application configuration module
        @param specific_modules_list build specific modules instead of all
        @param conditionals list of conditional modules to be added to the build
        @return list
    """
    if not hasattr(app, "modules"):
        raise RuntimeError("App's configuration file should have modules")
    conditionals.extend(get_conditionals_from_env(app))
    modules = {}
    if specific_modules and specific_modules[0] != '':
        app_modules = app.modules
        # if a conditional is in specific module list, add conditionals to list of modules
        if has_conditional_in_modules_list(app.conditional_modules, specific_modules):
            app_modules = get_module_merged_with_conditionals(app)
        fill_modlist_from_modules(app_modules, specific_modules, modules)
    else:
        modules = app.modules
    # Add specific conditional modules
    for modname in conditionals:
        add_conditional_module(app.conditional_modules, modules, modname)
    # Get every dependencies configuration for modules
    resolve_modules_dependencies(modules)
    return modules
