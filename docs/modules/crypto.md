# crypto

OpenSSL-backed cryptography: RSA and EC private keys, X.509 certificates, revocation checks (OCSP and CRL), a TLS context, symmetric ciphers, message digests and signatures. OpenSSL types stay out of the public headers; keys, certificates and contexts are type-erased handles, and failures are reported as `sihd::util::Result` errors carrying the OpenSSL error queue.

| | |
|---|---|
| Depends on | `sys` |
| External libraries | openssl |
| Platforms | all |
| Build | `make m=crypto` |
| Header | `#include <sihd/crypto/PrivateKey.hpp>` (also `Certificate.hpp`, `OcspRequest.hpp`, `Crl.hpp`, `RevocationChecker.hpp`, `TlsContext.hpp`, `cipher.hpp`, `digest.hpp`, `utils.hpp`) |

## Capabilities

### Keys

- `PrivateKey`: `generate_rsa(bits = 2048)` and `generate_ec(curve_name = "prime256v1")` create a key; `load_pem(path)`, `save_pem(path)`, `load_pem_string(pem)`, `to_pem_string()` read and write PEM; `clear()` releases the key. Keys are truthy when set (`operator bool`).

### Certificates

- `Certificate`: `generate_self_signed(key, common_name, days)` or `generate_self_signed(key, CertOptions)` creates a certificate; `generate_signed(key, CertOptions, issuer, issuer_key)` creates one issued by `issuer`. `CertOptions` sets subject fields, subject alternative names (`DNS:` / `IP:` prefixes), validity, serial, CA flag, OCSP responder urls (authority information access) and CRL urls (distribution points).
- `load_pem(path)`, `save_pem(path)`, `load_pem_string(pem)`, `to_pem_string()`, `load_der(data, len)`: load and save.
- `verify(ca)`: checks the certificate was issued by `ca`.
- `subject_common_name()`, `issuer_common_name()`, `subject_alt_names()`, `ocsp_urls()`, `crl_urls()`, `not_before()`, `not_after()`, `serial_hex()`, `is_ca()`: inspection.

### Revocation

- `RevocationInfo`: `status` (`good`, `revoked`, `unknown`), `source` (`none`, `ocsp`, `crl`), `this_update`, `next_update` (0 when absent), `revocation_time`.
- `OcspRequest`: `build(cert, issuer)` makes a request with a fresh nonce, `to_der()` encodes it for an HTTP POST. `verify_response(data, len, chain)` checks the responder's answer: signed by the issuer or a delegated responder chaining to the last certificate of `chain`, same nonce when the responder echoes one, within its validity period (5 minutes of clock skew), then returns the status of the requested certificate.
- `Crl`: `load_der(data, len)`, `verify(issuer)` (issuer name, signature, validity period; a delta CRL is refused with `not_supported`), `status(cert)` (`unknown` for a certificate of another issuer, or outside the partition an issuing distribution point restricts the CRL to), `this_update()`, `next_update()`.
- `RevocationChecker`: `check(chain)` on a chain ordered certificate, issuer, ..., trust anchor (the order `net::TlsConnection::peer_certificates` returns). `set_mode` picks the sources: `disabled` (no lookup, `unknown` from source `none`), `crl`, `ocsp` (strict: the OCSP answer is final, `unknown` included, CRLs are never asked) or `ocsp_then_crl` (default, best effort: the verified OCSP answer first, then the CRLs when every responder failed or one answered `unknown`; an `unknown` OCSP answer stands when no CRL answers). Within each source, urls come from the peer, so only `http://` ones reach a fetcher, others are skipped. A certificate naming no usable source gets `unknown` from source `none`, an error comes back only when every named source failed: `invalid_argument` when one of them answered with a forged or stale answer, else the code of the last failure. The transport is the caller's: `set_ocsp_fetcher` POSTs a DER request and returns the DER response, `set_crl_fetcher` GETs a DER CRL; an unset fetcher skips its source. OCSP answers are cached per certificate, CRLs per url, until their `next_update`, capped by `set_max_cache_age` (one hour by default). Thread-safe; nothing is logged, the hard or soft-fail policy on `unknown` stays with the caller.

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

Revocation check with `curl` as transport (an illustration, compiled but not part of the test suite):

```cpp
#include <sihd/crypto/RevocationChecker.hpp>
#include <sihd/curl/HeaderList.hpp>
#include <sihd/curl/Request.hpp>

sihd::util::Result<std::vector<uint8_t>> fetch(const std::string & url, sihd::util::ArrUByteView body)
{
    std::vector<uint8_t> answer;
    sihd::curl::Request request;
    (void)request.set_url(url);
    (void)request.set_proxy("");
    if (!body.empty())
    {
        sihd::curl::HeaderList headers;
        (void)headers.append("Content-Type: application/ocsp-request");
        (void)request.set_headers(headers);
        (void)request.set_body(body);
    }
    (void)request.set_write_callback([&](sihd::util::ArrByteView chunk) {
        answer.insert(answer.end(), chunk.data(), chunk.data() + chunk.size());
        return true;
    });
    auto performed = request.perform();
    if (!performed.has_value())
        return std::unexpected(std::move(performed).error());
    return answer;
}

void check(const std::vector<sihd::crypto::Certificate> & chain)
{
    sihd::crypto::RevocationChecker checker;
    checker.set_ocsp_fetcher(fetch);
    checker.set_crl_fetcher([](const std::string & url) { return fetch(url, {}); });
    auto info = checker.check(chain);
    // info->status == RevocationStatus::good, info->source == RevocationSource::ocsp
}
```

## Scripting

Not exposed to Lua or Python.
