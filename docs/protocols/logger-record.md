# Logger record protocol

What a [net](../modules/net.md#log-shipping) log writer sends and a log reader decodes: one log record, the same bytes over TCP, UDP and unix sockets. `LoggerRecord` in `modules/net/include/sihd/net/LoggerRecord.hpp` encodes and decodes it.

## Record, version 1

Every number is unsigned and big-endian (high byte first).

| Bytes | Field | Meaning |
|---|---|---|
| 1 | version | `1`; a reader refuses any other with `not_supported` |
| 1 | level | `LogLevel`: 0 emergency, 1 alert, 2 critical, 3 error, 4 warning, 5 notice, 6 info, 7 debug; above 7 is refused |
| 8 | timestamp | nanoseconds since the Unix epoch, when the record was logged |
| 2 | source length | then the source bytes, at most 65535 (longer is cut) |
| 2 | thread name length | then the thread name bytes, at most 65535 |
| 4 | message length | then the message bytes |

Nothing follows the message: a record with bytes left is refused, like a truncated one (`invalid_argument`). The texts are not NUL-terminated.

Example: source `ab`, thread `t`, message `hi`, level info, timestamp `0x0102030405060708`:

```
01                         version 1
06                         level info
01 02 03 04 05 06 07 08    timestamp
00 02 61 62                source "ab"
00 01 74                   thread name "t"
00 00 00 02 68 69          message "hi"
```

## Transports

- TCP and unix stream: each record is prefixed by its size as a 4-byte big-endian number. A reader drops a client announcing more than `LoggerReaderTcp::max_record_size` (16 MiB).
- UDP and unix datagram: one record per datagram, no prefix. `LoggerReaderUdp` reads at most `max_record_size` (64 KiB) per datagram.

The reader logs the record with its own thread id: the sender's thread id is not sent.
