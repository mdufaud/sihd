#ifndef __CRYPTO_TEST_HELPERS_HPP__
#define __CRYPTO_TEST_HELPERS_HPP__

#include <sihd/crypto/PrivateKey.hpp>

namespace test
{

inline sihd::crypto::PrivateKey generate_rsa_key()
{
    sihd::crypto::PrivateKey key;
    (void)key.generate_rsa(2048);
    return key;
}

// rsa generation dominates the suite: generated once per process, copies share the EVP_PKEY
inline const sihd::crypto::PrivateKey & rsa_key()
{
    static const sihd::crypto::PrivateKey key = generate_rsa_key();
    return key;
}

inline const sihd::crypto::PrivateKey & other_rsa_key()
{
    static const sihd::crypto::PrivateKey key = generate_rsa_key();
    return key;
}

} // namespace test

#endif
