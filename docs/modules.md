# Modules

SIHD is split into modules. Each module is a library `libsihd_<module>` with headers under `<sihd/<module>/...>` and namespace `sihd::<module>`. Build a subset with `make m=<module>,<module>`; required sihd modules are built automatically.

| Module | Description | Dependencies |
|--------|-------------|--------------|
| [`json`](modules/json.md) | JSON parsing/serialization | simdjson |
| [`util`](modules/util.md) | Base: logging, services, threads, scheduling, observers, CLI, time, containers, named nodes | json, fmt, cli11, pcre2 |
| [`sys`](modules/sys.md) | OS abstraction: files, processes, IPC, signals, dynamic loading, clipboard, screenshot, input injection, desktop notifications, users, Linux/Windows-only APIs | util |
| [`core`](modules/core.md) | Devices, Channels, recording/playback, sampling, profiling | util, sys |
| [`crypto`](modules/crypto.md) | Keys, certificates, TLS context, ciphers, digests, signatures | util, sys, openssl |
| [`curl`](modules/curl.md) | libcurl transfer wrapper | util, curl |
| [`net`](modules/net.md) | Sockets, TCP/UDP/ICMP, TLS, DNS, interfaces, routes, VPN detection, network monitoring | sys, core, crypto |
| [`http`](modules/http.md) | HTTP/WebSocket server, HTTP client | curl, net, libwebsockets, libuv, zlib, openssl |
| [`pcap`](modules/pcap.md) | Packet capture, injection and capture files | net, libpcap |
| [`csv`](modules/csv.md) | CSV reading/writing | util, sys |
| [`zip`](modules/zip.md) | Archive compression and encryption | util, sys, libzip |
| [`tui`](modules/tui.md) | Terminal UI components (ftxui) | util, sys, ftxui |
| [`ssh`](modules/ssh.md) | SSH client and server, SFTP | util, sys, libssh |
| [`usb`](modules/usb.md) | USB device enumeration, udev hardware names | util, sys, libusb |
| [`bt`](modules/bt.md) | Bluetooth LE scanning | util, sys |
| [`wifi`](modules/wifi.md) | Wifi link, access points, scans and connections through NetworkManager, iwd, wpa_supplicant or the Windows WLAN service | util, sys, net |
| [`imgui`](modules/imgui.md) | Dear ImGui runner (GLFW, SDL3, Win32, Android, ncurses / OpenGL3, DirectX11, ncurses) | util, sys, imgui, opengl |
| [`lua`](modules/lua.md) | Lua bindings (conditional: `lua=1`) | util, lua 5.3, luabridge3 |
| [`luabin`](modules/luabin.md) | Standalone Lua interpreter binary (conditional: `lua=1`) | lua |
| [`py`](modules/py.md) | Python bindings via pybind11 (conditional: `py=1`) | pybind11, python3 |

Dependencies list sihd modules first, then external libraries (fetched with `make dep`).
