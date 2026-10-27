# csv

Delimited text reading and writing. The reader skips comment lines, splits each line into columns and can expose one column as a timestamp. The writer emits rows and comment lines, and tracks the current row and column. Helpers convert between CSV text, files and typed tuples. `CsvRecorder` records channels to a CSV file.

| | |
|---|---|
| Depends on | `sys`, `core` |
| External libraries | none |
| Platforms | all |
| Build | `make m=csv` |
| Header | `#include <sihd/csv/CsvReader.hpp>` (also `CsvWriter.hpp`, `utils.hpp`, `CsvRecorder.hpp`) |

## Capabilities

### Reading

- `CsvReader`: opens a file and reads it line by line.
- `open(path)`, `is_open()`, `close()`: file lifecycle.
- `set_delimiter(c)`, `set_commentary(c)`: delimiter (default `,`) and comment character (default `#`).
- `set_timestamp_col(n)`, `set_timestamp_format(format)`: column holding the timestamp of each line, and its format.
- `read_next()`: reads the next data line, returns `Result<bool>` (`false` at end of file); comment lines are skipped.
- `columns()`: columns of the last line read; the reference is overwritten by the next read.
- `get_read_timestamp()`, `get_read_data(view)`: `IReaderTimestamp` accessors for the current line.

### Writing

- `CsvWriter`: opens a file and writes rows.
- `open(path, append = false)`, `is_open()`, `close()`: file lifecycle; `append` keeps existing content.
- `set_delimiter(c)`, `set_commentary(c)`: delimiter and comment character.
- `write_row(values)` and `write_row(view)`: write a complete row followed by a line feed.
- `write(values)` and `write(view)`: write cells on the current row, separated by the delimiter.
- `new_row()`: ends the current row.
- `write_commentary(text)`: writes a comment line.
- `current_row()`, `current_col()`, `max_col()`: position and widest row written so far.

### Recording channels

- `CsvRecorder`: a `core::DevRecorder` handler, like `core::MemRecorder`. Point the recorder at it (`set_handler("..csv")`, conf `handler`) and pick the channels with `add_record_channel("ALIAS=PATH")` (conf `record`). Each record becomes the row `timestamp,channel,value` as soon as it happens: the channel timestamp in nanoseconds, the alias, then the values separated by spaces (bytes and chars in hexadecimal). Rows go straight to the file, line buffered, nothing is kept in memory.
- `set_path(path)` (conf `path`): `{date}` is replaced by the local time of each `start()` (`%Y-%m-%d_%H-%M-%S`), so a restart writes a new file; a fixed path is truncated on each start. The first row is the header `timestamp,channel,value`.
- `start()` fails without a path or when the file cannot be opened.

### Helpers

- `utils::escape_str(view)`: quotes a value and doubles embedded quotes.
- `utils::csv_from_string(content, remove_header, delimiter, comment_char)`: parses CSV text into rows of columns.
- `utils::read_csv(path, remove_header, delimiter, comment_char)`: same, from a file; returns `Result`.
- `utils::csv_to_str(columns, rows, delimiter)`: renders a header and tuple rows to text.
- `utils::write_csv(path, columns, rows, delimiter)`: writes a header and tuple rows to a file; checks that each write succeeded.
- `utils::same_number_of_columns(columns, rows)`: checks header and tuple sizes match.

## Example

Adapted from `modules/csv/test/TestCsv.cpp`.

```cpp
#include <sihd/csv/CsvReader.hpp>
#include <sihd/csv/CsvWriter.hpp>

void write_then_read(const std::string & path)
{
    sihd::csv::CsvWriter writer;
    if (!writer.open(path).has_value())
        return;

    (void)writer.write_commentary("hello world");
    (void)writer.write_row({"1", "2", "3"});
    (void)writer.write_row({"4", "5"});
    (void)writer.close();

    sihd::csv::CsvReader reader;
    if (!reader.open(path).has_value())
        return;

    while (true)
    {
        auto res = reader.read_next();
        if (!res.has_value() || !res.value())
            break;
        const auto & cols = reader.columns();
        // cols[0] is "1" on the first line, then "4"
    }
}
```

## Scripting

Exposed to Lua (requires `lua=1`, module `sihd.csv`). Not exposed to Python.
