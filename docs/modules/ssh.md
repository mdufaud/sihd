# ssh

SSH client and server built on libssh. The client side covers sessions, authentication, remote commands, interactive shells, SSH keys and SFTP file transfer. The server side is a service that accepts connections and delegates auth and channel requests to a handler, with pluggable subsystems for exec, PTY shells, SFTP and in-process functions.

| | |
|---|---|
| Depends on | `util`, `sys` |
| External libraries | libssh |
| Platforms | all except Android |
| Build | `make m=ssh` |
| Header | `#include <sihd/ssh/SshSession.hpp>` (one header per class) |

## Capabilities

### Library

- `sihd::ssh::utils`: `init`, `is_initialized`, `finalize` manage the libssh library lifetime. Called internally by the module.
- `WinSize`: terminal size (rows, columns, pixel sizes) for PTY requests and resizes.
- `KeyType`, `SshKey::type_from_name`, `SshKey::type_str`: key algorithm enumeration and conversion.

### Client session

- `SshSession`: owns a libssh session. `fast_connect(ConnectOptions)` connects in one call; `new_session`, `set_user`, `set_host`, `set_port`, `set_timeout`, `set_process_config`, `set_ssh_dir` configure it before connecting.
- Authentication: `auth_password`, `auth_key`, `auth_key_file`, `auth_key_auto`, `auth_agent`, `auth_interactive_keyboard`, `auth_none`. Each returns `SshSession::AuthState` (`success`, `denied`, `partial`, `again`).
- Host keys: `check_hostkey`, `update_known_hosts`, `known_hosts`.
- Channels: `make_channel`, `make_channel_session`, `make_command`, `make_shell`, `make_sftp`.
- `disconnect`, `silent_disconnect`, `connected`, `get_banner`.

### Keys

- `SshKey`: `generate(KeyType, parameter)`, `import_pubkey_file`, `import_privkey_file`, `import_pubkey_mem`, `import_privkey_mem`, `export_privkey_file`, `base64`, `is_equal`.

### Channels

- `SshChannel`: low-level libssh channel. Opens sessions, forwards (`open_forward`, `open_forward_unix`, `open_x11`), requests PTY, shell, exec, subsystem or SFTP, and reads and writes with poll and timeout variants. Also `send_eof`, `send_signal`, `exit_status`.

### Commands and shells

- `SshCommand`: runs one remote command. `execute`, `execute_async`, `input`, `wait`, `exit_status`, `core_dumped`, `exit_signal_str`. Output is delivered to `output_handler`, with `is_stderr` set for stderr.
- `SshShell`: interactive shell. `open(x11)`, `read_loop`, `close`.

### SFTP client

- `Sftp`: `open`, `close`, `list_dir`, `list_dir_filenames`, `send_file`, `get_file`, `mkdir`, `rmdir`, `rm`, `rename`, `chmod`, `chown`, `symlink`, `readlink`, `extensions`, `version`.
- `SftpAttribute`: directory entry with `name`, `is_dir`, `is_file`, `is_link`, `size`.

### Server

- `SshServer`: service that listens for connections. Configure with `set_port`, `set_bind_address`, `set_rsa_key`, `set_ecdsa_key`, `set_authorized_keys_file`, `set_banner`, `set_kex_timeout`, `set_verbosity`, then set a handler with `set_server_handler`. `get_port` returns the bound port.
- `ISshServerHandler`: interface for auth decisions, session and channel events, channel data and PTY resize.
- `BasicSshServerHandler`: ready-to-use handler. `add_allowed_user`, `add_allowed_pubkey`, optional password and pubkey callbacks, shell, exec and subsystem factory callbacks, `counters`.

### Server subsystems

- `ISshSubsystemHandler`: interface for a channel's handler (`on_start`, `on_data`, `on_resize`, `on_eof`, `on_close`).
- `SshSubsystemExec`: runs a command as a process, in fork mode (streamed) or synchronous mode. Shell or direct parse mode.
- `SshSubsystemPty`: spawns a shell in a pseudo-terminal bridged to the channel. `is_supported` tells whether PTY is available.
- `SshSubsystemSftp`: SFTP server over a local directory set with `set_root_path`, with path traversal protection.
- `SshSubsystemFunction`: runs a `std::function` on the command string and returns output and exit code, without spawning a process.

## Example

```cpp
#include <sihd/ssh/SshCommand.hpp>
#include <sihd/ssh/SshSession.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/time.hpp>

using namespace sihd::util;
using namespace sihd::ssh;

int run_remote(const std::string & user, const std::string & password, const std::string & host)
{
    SshSession session;
    if (!session.fast_connect({.user = user, .host = host}).has_value())
        return -1;

    SshSession::AuthState auth_state = session.auth_password(password);
    if (!auth_state.success())
        return -1;

    std::string stdout_str;
    Handler<std::string_view, bool> output_handler([&stdout_str](std::string_view buf, bool is_stderr) {
        if (!is_stderr)
            stdout_str += buf;
    });

    SshCommand cmd = session.make_command();
    cmd.output_handler = &output_handler;
    if (!cmd.execute("uname -a").has_value())
        return -1;
    if (!cmd.wait(Duration(time::seconds(5))).has_value())
        return -1;

    return cmd.exit_status();
}
```

Adapted from `modules/ssh/demo/ssh_cmd_demo.cpp`.

## Scripting

Not exposed to Lua or Python.
