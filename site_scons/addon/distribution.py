###############################################################################
# distribution
###############################################################################

description = "Simple Input Handler Displayer"
license = "GPL3"
section = "libdevel"
priority = "optional"
architecture = "any"
multi_architecture = "same"
maintainers = ["mdufaud <maxence_dufaud@hotmail.fr>"]
contributors = ["azouiten <alexandre.zouiten1@gmail.com>"]

## APT

# packages equivalent to build DEBIAN/control dependencies
apt_packages = {
    "gtest": "libgtest-dev",
    "nlohmann-json": "nlohmann-json3-dev",
    "fmt": "libfmt-dev",
    "libuuid": "uuid-dev",
    "cli11": "cli11-dev",
    "openssl": "libssl-dev",
    "curl": "libcurl4-openssl-dev",
    "libwebsockets": "libwebsockets-dev",
    "libcap": "libcap-dev",
    "libuv": "libuv1-dev",
    "zlib": "zlib1g-dev",
    "libpcap": "libpcap-dev",
    "libssh": "libssh-dev",
    "libusb": "libusb-1.0-0-dev",
    "libzip": "libzip-dev",
    "simpleble": "libsimpleble-dev",
    "pybind11": "python3-pybind11",
    "opengl": "libgl-dev",
    "ftxui": "libftxui-dev",
    "glfw3": "libglfw3-dev",
    "glew": "libglew-dev",
    "sdl3": "libsdl3-dev",
    "lua": "liblua5.3-dev",
    "libjpeg": "libjpeg-dev",
    "libxcrypt": "libcrypt-dev",
    "python3": "python3-dev",
    "x11": "libx11-dev",
    "wayland": "libwayland-dev",
}

## PACMAN

# packages equivalent to build PKGBUILD dependencies
pacman_packages = {
    "gtest": "gtest",
    "nlohmann-json": "nlohmann-json",
    "fmt": "fmt",
    "libuuid": "util-linux-libs",
    "cli11": "cli11",
    "openssl": "openssl",
    "curl": "curl",
    "libwebsockets": "libwebsockets",
    "libcap": "libcap",
    "libuv": "libuv",
    "zlib": "zlib",
    "libpcap": "libpcap",
    "libssh": "libssh",
    "libusb": "libusb",
    "libzip": "libzip",
    "simpleble": "simpleble",
    "pybind11": "pybind11",
    "opengl": "mesa",
    # "ftxui": "ftxui",
    "glfw3": "glfw",
    "glew": "glew",
    "sdl3": "sdl3",
    "lua": "lua",
    "libjpeg": "libjpeg-turbo",
    "libxcrypt": "libxcrypt",
    "python3": "python",
    "x11": "libx11",
    "wayland": "wayland",
}

## YUM

yum_packages = {
    "gtest": "gtest-devel",
    "nlohmann-json": "json-devel",
    "fmt": "fmt-devel",
    "libuuid": "libuuid-devel",
    "cli11": "cli11-devel",
    "openssl": "openssl-devel",
    "curl": "libcurl-devel",
    "libwebsockets": "libwebsockets-devel",
    "libcap": "libcap-devel",
    "libuv": "libuv-devel",
    "zlib": "zlib-devel",
    "libpcap": "libpcap-devel",
    "libssh": "libssh-devel",
    "libusb": "libusb1-devel",
    "libzip": "libzip-devel",
    "simpleble": "simpleble-devel",
    "pybind11": "pybind11-devel",
    "opengl": "mesa-libGL-devel",
    "ftxui": "ftxui-devel",
    "glfw3": "glfw-devel",
    "glew": "glew-devel",
    "sdl3": "SDL3-devel",
    "lua": "lua-devel",
    "libjpeg": "libjpeg-turbo-devel",
    "libxcrypt": "libxcrypt-devel",
    "python3": "python3-devel",
    "x11": "libX11-devel",
    "wayland": "wayland-devel",
}

def configure(app):
    app.pacman_source = "{name}-{version}::git+{git_url}#tag={version}".format(
        name=app.name,
        version=app.version,
        git_url=app.git_url,
    )
