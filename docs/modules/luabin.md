# luabin

`sihd_lua` is a line-based Lua REPL on top of the `lua` module. It loads every sihd Lua binding compiled in the build, then reads standard input one line at a time and runs each line as a Lua chunk.

| | |
|---|---|
| Depends on | `lua` |
| External libraries | lua 5.3 |
| Platforms | all |
| Build | `make lua=1 m=util,sys,core,lua,luabin` |
| Header | none (executable, source in `modules/luabin/src/main.cpp`) |

## Capabilities

- Loads the util, sys, json, csv, zip, core, net and http bindings that are compiled in, through `SIHD_LUA_WITH_<MODULE>` from the generated `lua` config header.
- Sets `package.path` to `<executable dir>/etc/sihd/lua/?.lua` and runs `require 'luabin.preload'`, which defines `table_print(tbl)` (prints each key and value). The `etc` directory is copied next to `bin` by the build.
- Prints the prompt `$> ` and runs each input line with `Vm::do_string`. A Lua error is written to stderr with its code and does not stop the loop. Input ends at EOF.
- Each line is a separate chunk: a statement that spans several lines must be written on one line.

Build and run (from the repository root, after the build above):

```sh
LD_LIBRARY_PATH=build/last/lib build/last/bin/sihd_lua
echo 'print(sihd.version)' | LD_LIBRARY_PATH=build/last/lib build/last/bin/sihd_lua
```

`build/last` is the symlink to the most recent build directory.

## Example

Not applicable: `luabin` is an executable without a public API.

## Scripting

Not applicable: `luabin` is a host program. Scripts reach the same bindings as with `lua`, documented in `docs/modules/lua.md`.
