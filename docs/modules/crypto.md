# crypto

OpenSSL-backed cryptography: RSA and EC private keys, X.509 certificates, a TLS context, symmetric ciphers, message digests and signatures. OpenSSL types stay out of the public headers; keys, certificates and contexts are type-erased handles, and failures are reported as `sihd::util::Result` errors carrying the OpenSSL error queue.

| | |
|---|---|
| Depends on | `sys` |
| External libraries | openssl |
| Platforms | all |
| Build | `make m=crypto` |
| Header | `#include <sihd/crypto/PrivateKey.hpp>` (also `Certificate.hpp`, `TlsContext.hpp`, `cipher.hpp`, `digest.hpp`, `utils.hpp`) |

## Capabilities

### Keys

- `PrivateKey`: `generate_rsa(bits = 2048)` and `generate_ec(curve_name = "prime256v1")` create a key; `load_pem(path)`, `save_pem(path)`, `load_pem_string(pem)`, `to_pem_string()` read and write PEM; `clear()` releases the key. Keys are truthy when set (`operator bool`).

### Certificates

- `Certificate`: `generate_self_signed(key, common_name, days)` or `generate_self_signed(key, CertOptions)` creates a certificate; `CertOptions` sets subject fields, subject alternative names (`DNS:` / `IP:` prefixes), validity, serial, CA flag.
- `load_pem(path)`, `save_pem(path)`, `load_pem_string(pem)`, `to_pem_string()`, `load_der(data, len)`: load and save.
- `verify(ca)`: checks the certificate was issued by `ca`.
- `subject_common_name()`, `issuer_common_name()`, `subject_alt_names()`, `not_before()`, `not_after()`, `serial_hex()`, `is_ca()`: inspection.

### TLS

- `TlsContext`: `init(server_mode)` creates a context (TLS 1.2 minimum, renegotiation refused by default).
- `set_certificate(cert)`, `set_private_key(key)`, `load_ca_cert(path)`: identity and trust store.
- `set_verify_peer(verify)`: peer certificate verification; on by default for clients, requires a client certificate on servers.
- `set_session_id_context(context)`: session resumption scope for servers.
- `set_renegotiation(allow)`: opt in to TLS renegotiation.

### Ciphers and digests

- `cipher::encrypt(algorithm, key, iv, data)` and `cipher::decrypt(...)`: symmetric encryption by algorithm name (for example AES-256-CBC), with raw pointer or `ArrByteView` arguments.
- `digest::compute(algorithm, data)`: hash by algorithm name (for example `SHA-256`), raw pointer or `ArrByteView` arguments.

### Signatures

- `utils::sign_data(key, data)`: signs data with a private key.
- `utils::verify_data(cert, data, signature)`: verifies a signature with a certificate's public key.

## Example

Adapted from `modules/crypto/test/TestUtils.cpp` and `modules/crypto/test/TestCipherUtils.cpp`.

```cpp
#include <string>

#include <sihd/crypto/Certificate.hpp>
#include <sihd/crypto/PrivateKey.hpp>
#include <sihd/crypto/digest.hpp>
#include <sihd/crypto/utils.hpp>

void sign_and_hash()
{
    sihd::crypto::PrivateKey key;
    if (!key.generate_rsa(2048).has_value())
        return;

    sihd::crypto::Certificate cert;
    if (!cert.generate_self_signed(key, "test.local").has_value())
        return;

    const std::string message = "test data to sign";
    const auto * data = reinterpret_cast<const uint8_t *>(message.data());

    auto signature = sihd::crypto::utils::sign_data(key, data, message.size());
    if (!signature.has_value())
        return;

    bool valid = sihd::crypto::utils::verify_data(cert, data, message.size(), signature->data(), signature->size());

    auto hash = sihd::crypto::digest::compute("SHA-256", data, message.size());
    // valid == true, hash->size() == 32
}
```

## Scripting

Not exposed to Lua or Python.
