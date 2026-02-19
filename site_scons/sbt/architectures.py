"""
SBT Architecture Configuration for cross-compilation.
"""

machine_aliases = {
    "aarch64": "arm64",
    "amd64": "x86_64",
    "i686": "i386",
    "armv7l": "arm32",
    "armv6l": "arm32",
    "arm": "arm32",
}

# fmt: off
architectures = {
    "x86_64":      {"gcc": {"gnu": "x86_64-linux-gnu-", "musl": "x86_64-linux-musl-"}, "zig": "x86_64-linux-musl", "vcpkg": "x64"},
    "x86":         {"gcc": {"gnu": "i686-linux-gnu-", "musl": "i686-linux-musl-"}, "zig": "i686-linux-musl", "vcpkg": "x86"},
    "i386":        {"gcc": {"gnu": "i686-linux-gnu-", "musl": "i686-linux-musl-"}, "zig": "i386-linux-musl", "vcpkg": "x86"},
    "arm32":       {"gcc": {"gnu": "arm-linux-gnueabihf-", "musl": "arm-linux-musleabihf-"}, "zig": "arm-linux-musleabihf", "zig_flags": "-mcpu=generic+v7a", "vcpkg": "arm"},
    "arm64":       {"gcc": {"gnu": "aarch64-linux-gnu-", "musl": "aarch64-linux-musl-"}, "zig": "aarch64-linux-musl", "vcpkg": "arm64"},
    "riscv64":     {"gcc": {"gnu": "riscv64-linux-gnu-", "musl": "riscv64-linux-musl-"}, "zig": "riscv64-linux-musl", "vcpkg": "riscv64"},
    "riscv32":     {"gcc": {"gnu": "riscv32-linux-gnu-", "musl": "riscv32-linux-musl-"}, "zig": "riscv32-linux-musl", "vcpkg": "riscv32"},
    "loongarch64": {"gcc": {"gnu": "loongarch64-linux-gnu-", "musl": "loongarch64-linux-musl-"}, "zig": "loongarch64-linux-musl", "vcpkg": "loongarch64"},
    "mips64el":    {"gcc": {"gnu": "mips64el-linux-gnuabi64-", "musl": "mips64el-linux-musl-"}, "zig": "mips64el-linux-musl", "vcpkg": "mips64"},
    "s390x":       {"gcc": {"gnu": "s390x-linux-gnu-", "musl": "s390x-linux-musl-"}, "zig": "s390x-linux-musl", "vcpkg": "s390x"},
    "ppc64le":     {"gcc": {"gnu": "powerpc64le-linux-gnu-", "musl": "powerpc64le-linux-musl-"}, "zig": "powerpc64le-linux-musl", "vcpkg": "ppc64le"},
}
# fmt: on

# Machine name used in GNU triplets (differs from internal machine name)
_gnu_machine_map = {
    "arm32": "arm",
    "arm64": "aarch64",
}

# Libraries that don't exist or are built-in with musl libc
musl_excluded_libs = ["pthread", "m", "dl", "rt", "crypt", "util", "xnet", "resolv"]


def normalize_machine(machine: str) -> str:
    return machine_aliases.get(machine, machine)


def get_config(machine: str) -> dict:
    return architectures.get(normalize_machine(machine), {})


def get_gnu_machine(machine: str) -> str:
    """Get the machine name as used in GNU triplets (e.g. arm32 -> arm, arm64 -> aarch64)"""
    return _gnu_machine_map.get(machine, machine)


def get_gcc_prefix(machine: str, libc: str) -> str:
    return get_config(machine).get("gcc", {}).get(libc, "")


def get_zig_target(machine: str) -> str:
    return get_config(machine).get("zig", "")


def get_zig_flags(machine: str) -> str:
    return get_config(machine).get("zig_flags", "")


def get_vcpkg_machine(machine: str) -> str:
    return get_config(machine).get("vcpkg", machine)
