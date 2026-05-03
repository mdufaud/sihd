import os
import shutil
import tarfile
import subprocess
from os.path import join

try:
    from sbt import builder as _b
    from sbt import logger
except ImportError:
    from site_scons.sbt import builder as _b
    from site_scons.sbt import logger

from site_scons.sbt.build import modules as sbt_modules
from site_scons.sbt.build import utils
from site_scons.sbt import architectures

###############################################################################
# TAR distribution
###############################################################################

def create_tar_package(app):
    os.makedirs(_b.build_dist_path, exist_ok = True)
    tar_path = join(_b.build_dist_path, "{}-{}.tar.gz".format(app.name, app.version))
    logger.info("compressing build to: " + tar_path)
    with tarfile.open(tar_path, "w:gz") as tar:
        # include
        if os.path.isdir(_b.build_hdr_path):
            tar.add(_b.build_hdr_path, arcname = os.path.basename(_b.build_hdr_path))
        # lib
        if os.path.isdir(_b.build_lib_path):
            tar.add(_b.build_lib_path, arcname = os.path.basename(_b.build_lib_path))
        # bin
        if os.path.isdir(_b.build_bin_path):
            tar.add(_b.build_bin_path, arcname = os.path.basename(_b.build_bin_path))
        # etc
        if os.path.isdir(_b.build_etc_path):
            tar.add(_b.build_etc_path, arcname = os.path.basename(_b.build_etc_path))
        # share
        if os.path.isdir(_b.build_share_path):
            tar.add(_b.build_share_path, arcname = os.path.basename(_b.build_share_path))
        # extlibs/include — merged into include/
        if os.path.isdir(_b.build_extlib_hdr_path):
            tar.add(_b.build_extlib_hdr_path, arcname = os.path.basename(_b.build_hdr_path))
        # extlibs/lib — merged into lib/
        if not _b.build_for_windows and os.path.isdir(_b.build_extlib_lib_path):
            tar.add(_b.build_extlib_lib_path, arcname = os.path.basename(_b.build_lib_path))
        # extlibs/bin — merged into bin/
        if os.path.isdir(_b.build_extlib_bin_path):
            tar.add(_b.build_extlib_bin_path, arcname = os.path.basename(_b.build_bin_path))

###############################################################################
# APT distribution
###############################################################################

def _get_deb_multiarch():
    """Return the Debian multiarch triplet, e.g. x86_64-linux-gnu.
    Queries dpkg-architecture when available, falls back to get_gnu_triplet()."""
    try:
        result = subprocess.run(
            ['dpkg-architecture', '-qDEB_HOST_MULTIARCH'],
            capture_output=True, text=True, timeout=5
        )
        if result.returncode == 0:
            return result.stdout.strip()
    except Exception:
        pass
    return _b.get_gnu_triplet()

def create_apt_package(app, modules):
    try:
        modules_extlibs = sbt_modules.get_modules_extlibs(app, modules, _b.build_platform)
        dependencies, missing_dep = sbt_modules.get_modules_packages(app, "apt", modules_extlibs)
    except SystemExit as err:
        logger.error(err)
        exit(1)
    apt_path = join(_b.build_dist_path, "apt", "{}-{}".format(app.name, app.version))
    debian_path = join(apt_path, "DEBIAN")
    control_path = join(debian_path, "control")
    logger.info("creating apt package: " + apt_path)
    # debian/control
    os.makedirs(debian_path, exist_ok = True)
    logger.info("creating apt control file: {}".format(control_path))
    with open(control_path, "w") as fd:
        fd.write("Package: {}\n".format(app.name))
        priority = hasattr(app, "priority") and app.priority or "optional"
        fd.write("Priority: {}\n".format(priority))
        fd.write("Maintainer: {}\n".format(app.maintainers[0]))
        all_contributors = app.maintainers[1:] + getattr(app, "contributors", [])
        if all_contributors:
            fd.write("Uploaders: {}\n".format(", ".join(all_contributors)))
        if hasattr(app, "url"):
            fd.write("Homepage: {}\n".format(app.url))
        if hasattr(app, "section"):
            fd.write("Section: {}\n".format(app.section))
        fd.write("Architecture: {}\n".format(app.architecture))
        if hasattr(app, "multi_architecture"):
            fd.write("Multi-Arch: {}\n".format(app.multi_architecture))
        fd.write("Version: {}\n".format(app.version))
        if dependencies:
            # Depends: libname (>= version), other_libname (>= other_version)
            fd.write("Depends: {}\n".format(", ".join(["{} (>= {})".format(k, v.split('#')[0]) for k, v in dependencies.items()])))
        fd.write("Description: {}\n".format(app.description))
    # /usr/bin
    apt_binary_path = join(apt_path, "usr", "bin")
    if os.path.isdir(_b.build_bin_path):
        os.makedirs(apt_binary_path, exist_ok = True)
        shutil.copytree(_b.build_bin_path, apt_binary_path, dirs_exist_ok = True)
    # /usr/include
    apt_include_path = join(apt_path, "usr", "include")
    if os.path.isdir(_b.build_hdr_path):
        os.makedirs(apt_include_path, exist_ok = True)
        shutil.copytree(_b.build_hdr_path, apt_include_path, dirs_exist_ok = True)
    # /usr/share
    apt_share_path = join(apt_path, "usr", "share")
    if os.path.isdir(_b.build_share_path):
        os.makedirs(apt_share_path, exist_ok = True)
        shutil.copytree(_b.build_share_path, apt_share_path, dirs_exist_ok = True)
    # /usr/lib/<multiarch-triplet> — prefer dpkg-architecture if available
    _deb_multiarch = _get_deb_multiarch()
    apt_lib_path = join(apt_path, "usr", "lib", _deb_multiarch)
    if os.path.isdir(_b.build_lib_path):
        os.makedirs(apt_lib_path, exist_ok = True)
        shutil.copytree(_b.build_lib_path, apt_lib_path, dirs_exist_ok = True)
    # /etc
    apt_etc_path = join(apt_path, "etc")
    if os.path.isdir(_b.build_etc_path):
        os.makedirs(apt_etc_path, exist_ok = True)
        shutil.copytree(_b.build_etc_path, apt_etc_path, dirs_exist_ok = True)
    # dpkg-deb to build package — use fakeroot so installed files are owned by root
    if shutil.which("dpkg-deb") is not None:
        if shutil.which("fakeroot") is not None:
            subprocess.call(['fakeroot', 'dpkg-deb', '--build', apt_path])
        else:
            logger.warning("fakeroot not found — package files will have wrong ownership")
            subprocess.call(['dpkg-deb', '--build', apt_path])
    else:
        logger.error("dpkg-deb is missing")

###############################################################################
# PACMAN distribution
###############################################################################

def create_pacman_package(app, modules):
    try:
        modules_extlibs = sbt_modules.get_modules_extlibs(app, modules, _b.build_platform)
        dependencies, missing_dep = sbt_modules.get_modules_packages(app, "pacman", modules_extlibs)
    except SystemExit as err:
        logger.error(err)
        exit(1)
    pacman_path = join(_b.build_dist_path, "pacman", "{}-{}".format(app.name, app.version))
    logger.info("creating pacman package: " + pacman_path)
    os.makedirs(pacman_path, exist_ok = True)
    pkg_build_path = join(pacman_path, "PKGBUILD")
    # change directory to pacman path
    old_cwd = os.getcwd()
    os.chdir(pacman_path)
    logger.info("creating pacman PKGBUILD: {}".format(pkg_build_path))
    # create PKGBUILD file
    with open(pkg_build_path, "w") as fd:
        for maintainer in app.maintainers:
            fd.write('# Maintainer: {}\n'.format(maintainer))
        for uploader in getattr(app, "contributors", []):
            fd.write("# Contributor: {}\n".format(uploader))
        fd.write('\n')
        fd.write('pkgname={}\n'.format(app.name))
        fd.write('pkgver={}\n'.format(app.version))
        fd.write('pkgrel={}\n'.format(1))
        fd.write('pkgdesc="{}"\n'.format(app.description))
        fd.write("arch=('{}')\n".format(architectures.get_pacman_arch(_b.build_machine)))
        if hasattr(app, "url"):
            fd.write('url="{}"\n'.format(app.url))
        if hasattr(app, "license"):
            fd.write("license=('{}')\n".format(app.license))
        if hasattr(app, "section"):
            fd.write("groups=('{}')\n".format(app.section))
        fd.write("makedepends=('git' 'make' 'scons')\n")
        if dependencies:
            # depends=('libname>=version' 'other_libname>=other_version')
            # strip pkgrel suffix (e.g. '1.0.3#15' -> '1.0.3')
            fd.write("depends=({})\n".format(" ".join(["'{}>={}'".format(k, v.split('#')[0]) for k, v in dependencies.items()])))
        if hasattr(app, "pacman_source"):
            fd.write('source=("{}")\n'.format(app.pacman_source))
        fd.write("sha512sums=('SKIP')\n\n")
        fd.write(('build() {{\n'
            '\tcd "${{srcdir}}/${{pkgname}}-${{pkgver}}"\n'
            '\tmake fclean\n'
            '\tmake modules={modules} asan={asan} static={static} '
            'platform={platform} compiler={compiler} machine={machine} libc={libc} mode={mode} ')
        .format(
            modules = utils.get_opt("modules"),
            asan = utils.get_opt("asan", "0"),
            static = utils.get_opt("static", "0"),
            platform = _b.build_platform,
            compiler = _b.build_compiler,
            machine = _b.build_machine,
            libc = _b.libc,
            mode = _b.build_mode,
        ))
        if hasattr(app, "additional_build_env"):
            fd.write(" ".join(["{}={}".format(k, utils.get_opt(k, "0")) for k in app.additional_build_env]))
        else:
            # Auto-deduce additional env vars from module configs:
            # - "conditional-env" keys from conditional_modules (e.g. py=1, lua=1)
            # - "env" dict keys from all modules (e.g. x11=1, wayland=1)
            extra_env_keys = set()
            for mod_conf in getattr(app, "conditional_modules", {}).values():
                cond_env = mod_conf.get("conditional-env")
                if cond_env:
                    extra_env_keys.add(cond_env)
            for mod_conf in getattr(app, "modules", {}).values():
                for env_key in mod_conf.get("env", {}).keys():
                    extra_env_keys.add(env_key)
            if extra_env_keys:
                fd.write(" ".join(["{}={}".format(k, utils.get_opt(k, "0")) for k in sorted(extra_env_keys)]))
        fd.write('\n}\n')
        fd.write('\n')
        fd.write(('package() {{\n'
            '\tcd "${{srcdir}}/${{pkgname}}-${{pkgver}}"\n'
            '\tmake install INSTALL_DESTDIR="${{pkgdir}}" INSTALL_PREFIX="/usr" INSTALL_NOCONFIRM=1 '
            'platform={platform} compiler={compiler} machine={machine} libc={libc} mode={mode}\n'
        '}}\n').format(
            platform = _b.build_platform,
            compiler = _b.build_compiler,
            machine = _b.build_machine,
            libc = _b.libc,
            mode = _b.build_mode,
        ))
    # generate .SRCINFO if makepkg is available
    srcinfo_path = join(pacman_path, ".SRCINFO")
    if shutil.which("makepkg") is not None:
        result = subprocess.run(["makepkg", "--printsrcinfo"], capture_output=True, text=True)
        if result.returncode == 0:
            with open(srcinfo_path, "w") as fd:
                fd.write(result.stdout)
            logger.info("generated .SRCINFO at: {}".format(srcinfo_path))
        else:
            logger.warning("makepkg --printsrcinfo failed: {}".format(result.stderr.strip()))
    else:
        logger.warning(
            "makepkg not found — generate .SRCINFO manually:\n"
            "  cd {} && makepkg --printsrcinfo > .SRCINFO".format(pacman_path)
        )
    # change back to old directory
    os.chdir(old_cwd)

###############################################################################
# Docker distribution
###############################################################################

def _get_docker_modules_str(modules):
    """Return a compact string suitable for use as a directory name."""
    return "-".join(sorted(modules.keys()))


def _get_docker_make_args(modules, has_demo):
    """Return a list of make variable assignments for the docker build stage."""
    mods_str = ",".join(sorted(modules.keys()))
    args = [f"m={mods_str}", "mode=release"]
    if has_demo:
        args.append("demo=1")
    return args


def create_docker_package(app, modules):
    """Generate a multi-stage Dockerfile (debian:bookworm-slim builder + alpine runtime).

    Output: dist/docker/Dockerfile
    Usage:  docker build -t <app>-<modules> dist/docker/
    """
    has_demo = _b.build_demo

    try:
        modules_extlibs = sbt_modules.get_modules_extlibs(app, modules, _b.build_platform)
        # Include demo_extlibs when demo=1 (build stage only)
        demo_extlibs = {}
        if has_demo and hasattr(app, "demo_extlibs"):
            demo_extlibs = sbt_modules.get_extlibs_versions(app, app.demo_extlibs)
        build_extlibs = dict(modules_extlibs)
        build_extlibs.update(demo_extlibs)
        # Build stage: apt -dev packages (modules + demo)
        build_deps, missing_build = sbt_modules.get_modules_packages(app, "apt", build_extlibs)
        # Runtime stage: apk packages — demo_extlibs excluded (header-only / build tools)
        if hasattr(app, "apk_packages"):
            runtime_deps, missing_runtime = sbt_modules.get_modules_packages(app, "apk", modules_extlibs)
        else:
            runtime_deps = {}
            missing_runtime = list(modules_extlibs.keys())
    except SystemExit as err:
        logger.error(err)
        exit(1)

    if missing_build:
        logger.warning("apt build packages not found for: {} — will use make dep".format(", ".join(missing_build)))
    if missing_runtime:
        logger.warning("apk runtime packages not found for: {}".format(", ".join(missing_runtime)))

    make_args = _get_docker_make_args(modules, has_demo)
    make_args_str = " ".join(make_args)
    # For make dep: skip all extlibs already provided by apt
    dep_make_args = make_args + ["with-system-packages=apt"]
    dep_make_args_str = " ".join(dep_make_args)
    modules_str = _get_docker_modules_str(modules)

    docker_path = join(_b.build_dist_path, "docker")
    os.makedirs(docker_path, exist_ok=True)
    dockerfile_path = join(docker_path, "Dockerfile")  # overwritten on each run

    # Toolchain packages always needed in the builder stage
    builder_base_pkgs = [
        "git", "make", "python3", "scons",
        "g++", "pkg-config", "ca-certificates",
    ]
    # If some extlibs are missing from apt, vcpkg will be used — add its prerequisites
    if missing_build:
        builder_base_pkgs += ["curl", "zip", "unzip", "tar", "cmake", "ninja-build"]
    # Add -dev packages resolved from module extlibs
    all_build_pkgs = builder_base_pkgs + sorted(build_deps.keys())
    build_pkgs_line = " \\\n        ".join(all_build_pkgs)

    git_url = getattr(app, "git_url", "")
    app_version = getattr(app, "version", "main")

    runtime_pkgs_line = " \\\n        ".join(sorted(runtime_deps.keys())) if runtime_deps else ""

    lines = [
        "# Generated by SBT — do not edit manually",
        "# Build: docker build -t {app}-{mods} .".format(app=app.name, mods=modules_str),
        "# Run:   docker run --rm {app}-{mods} <binary>".format(app=app.name, mods=modules_str),
        "",
        "###############################################################################",
        "# Stage 1: builder (debian:bookworm-slim)",
        "###############################################################################",
        "FROM debian:bookworm-slim AS builder",
        "",
        "RUN apt-get update && apt-get install -y --no-install-recommends \\",
        "        {pkgs} \\".format(pkgs=build_pkgs_line),
        "    && rm -rf /var/lib/apt/lists/*",
        "",
        "RUN git clone --depth 1 {url} /src".format(url=git_url),
        "",
        "WORKDIR /src",
        "",
    ]

    if missing_build:
        lines += [
            "RUN make dep {args}".format(args=dep_make_args_str),
            "",
        ]

    lines += [
        "RUN make {args}".format(args=make_args_str),
        "",
        "RUN make install INSTALL_DESTDIR=/dist INSTALL_PREFIX=/usr",
        "",
        "###############################################################################",
        "# Stage 2: runtime (alpine:latest)",
        "###############################################################################",
        "FROM alpine:latest",
        "",
    ]

    if runtime_pkgs_line:
        lines += [
            "RUN apk add --no-cache \\",
            "        {pkgs}".format(pkgs=runtime_pkgs_line),
            "",
        ]

    lines += [
        "COPY --from=builder /dist/usr/bin/  /usr/bin/",
        "COPY --from=builder /dist/usr/lib/  /usr/lib/{app}/".format(app=app.name),
        "COPY --from=builder /dist/etc/      /etc/",
        "",
        "ENV LD_LIBRARY_PATH=/usr/lib/{app}".format(app=app.name),
    ]

    if has_demo:
        lines += [
            "",
            "COPY --from=builder /dist/usr/share/ /usr/share/",
        ]

    content = "\n".join(lines) + "\n"
    with open(dockerfile_path, "w") as fd:
        fd.write(content)
    logger.info("generated Dockerfile at: {}".format(dockerfile_path))

    # .dockerignore — placed next to the Dockerfile (overwritten on each run)
    dockerignore_path = join(docker_path, ".dockerignore")
    with open(dockerignore_path, "w") as fd:
        fd.write("# Generated by SBT\n")
        fd.write("build/\n")
        fd.write("dist/\n")
        fd.write(".vcpkg/\n")
        fd.write(".venv/\n")
        fd.write("*.pyc\n")
        fd.write("__pycache__/\n")
    logger.info("generated .dockerignore at: {}".format(dockerignore_path))


###############################################################################
# Distribution
###############################################################################

def distribute_app(app, modules):
    dist_type = utils.get_opt("dist", None)
    if dist_type is None:
        raise SystemExit("cannot distribute app without its type")
    logger.info("creating {} distribution for app {}".format(dist_type, app.name))
    if dist_type == "tar":
        create_tar_package(app)
    elif dist_type == "apt":
        create_apt_package(app, modules)
    elif dist_type == "pacman":
        create_pacman_package(app, modules)
    elif dist_type == "docker":
        create_docker_package(app, modules)
    else:
        raise SystemExit("cannot distribute app type: {}".format(dist_type))
