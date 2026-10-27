# zip

ZIP archive creation, extraction, inspection and editing on libzip. `ZipFile` is a handle on one archive: it reads entries, adds or edits entries in memory and writes the result on `close()`. Free functions cover the common cases of zipping a directory, extracting an archive and listing entries.

| | |
|---|---|
| Depends on | `sys` |
| External libraries | libzip |
| Platforms | all |
| Build | `make m=zip` |
| Header | `#include <sihd/zip/ZipFile.hpp>` (also `zip.hpp`) |

## Capabilities

### One-shot functions

- `zip(root_dir_path, archive_path)`: archives a directory tree.
- `unzip(archive_path, unzip_file_path)`: extracts an archive into a directory.
- `list_entries(archive_path)`: returns the entry names of an archive.

### Archive handle

- `ZipFile(path, read_only = false, do_strict_checks = false)` and `open(...)`: open or create an archive; `is_open()` reports the state.
- `close()`: writes pending changes and closes; `discard()`: closes without saving.
- `comment_archive(comment)`, `archive_comment()`, `set_archive_readonly(active)`: archive-level comment and read-only flag.
- `count_original_entries()`, `count_entries()`: entries in the archive before and after pending additions.

### Reading entries

- `load_entry(index)` and `load_entry(name)`: select an entry; `current_entry()`, `entry_name()`, `entry_index()`, `entry_comment()`, `is_entry_directory()`: inspect the selected entry (`ZipEntry` holds name, size, compressed size, time and CRC).
- `read_next_entry()`: advances through the original entries.
- `read_entry(password = "")`: reads a chunk of the selected entry into the internal buffer, returns the byte count, `0` at end; `get_read_data(view)` gives access to that chunk.
- `read_next()`: `IReader` variant of `read_entry`, returns `Result<bool>`.
- `set_buffer_size(size)`: size of the internal read buffer.
- `dump_entry_to_fs(path, password = "")`: writes the selected entry to disk, or creates a directory for a directory entry.

### Editing entries

- `add_dir(name)`, `add_file(name, view)`: add entries in memory; the view must stay valid until `close()` or `discard()`.
- `add_from_fs(name, path)`, `add_file_from_fs(name, path)`, `add_dir_from_fs(name, path)`: add a file or directory from disk, the directory variant recursively.
- `replace_entry(view)`: replaces the content of the selected entry.
- `rename_entry(name)`, `remove_entry()`, `unchange_entry()`: rename, delete or revert the selected entry.
- `modify_entry_time(timestamp)`, `comment_entry(comment)`: entry time and comment.

### Encryption

- `set_password(password)`: default password for encrypted content.
- `set_aes_encryption(aes)`: selects the method by bit size (`0`, `128`, `192`, `256`) used by `encrypt_entry` and `encrypt_all`; other values are refused.
- `no_encrypt()`, `encrypt_in_aes_128()`, `encrypt_in_aes_192()`, `encrypt_in_aes_256()`: set the method used by `encrypt_entry` and `encrypt_all`.
- `encrypt_entry(password)`, `encrypt_all(password)`: encrypt the selected entry or every current entry.

## Example

Adapted from `modules/zip/test/TestZipFile.cpp` and `modules/zip/test/TestZip.cpp`.

```cpp
#include <sihd/util/Array.hpp>
#include <sihd/zip/ZipFile.hpp>
#include <sihd/zip/zip.hpp>

void write_and_read(const std::string & zip_path)
{
    {
        sihd::zip::ZipFile writer(zip_path);
        if (!writer.is_open())
            return;
        if (!writer.add_dir("hello").has_value())
            return;
        if (!writer.add_file("hello/world", "Hello world !").has_value())
            return;
        if (!writer.close().has_value())
            return;
    }

    sihd::zip::ZipFile reader(zip_path, true);
    if (!reader.load_entry("hello/world").has_value())
        return;

    sihd::util::ArrCharView view;
    auto read = reader.read_entry();
    if (read.has_value() && read.value() > 0)
        (void)reader.get_read_data(view);
    // view.str() == "Hello world !"
}
```

## Scripting

Exposed to Lua (requires `lua=1`, module `sihd.zip`). Not exposed to Python.
