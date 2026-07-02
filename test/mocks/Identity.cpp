#include "Identity.h"
#include <string.h>

namespace mesh {

Identity::Identity() { memset(pub_key, 0, sizeof(pub_key)); }
Identity::Identity(const char*) { memset(pub_key, 0, sizeof(pub_key)); }
bool Identity::verify(const uint8_t*, const uint8_t*, int) const { return false; }
bool Identity::readFrom(Stream&) { return false; }
bool Identity::writeTo(Stream&) const { return false; }
void Identity::printTo(Stream&) const {}

LocalIdentity::LocalIdentity() { memset(prv_key, 0, sizeof(prv_key)); }
LocalIdentity::LocalIdentity(const char*, const char*) { memset(pub_key, 0, sizeof(pub_key)); memset(prv_key, 0, sizeof(prv_key)); }
LocalIdentity::LocalIdentity(RNG*) { memset(pub_key, 0, sizeof(pub_key)); memset(prv_key, 0, sizeof(prv_key)); }
bool LocalIdentity::validatePrivateKey(const uint8_t*) { return false; }
bool LocalIdentity::readFrom(Stream&) { return false; }
bool LocalIdentity::writeTo(Stream&) const { return false; }
void LocalIdentity::printTo(Stream&) const {}
size_t LocalIdentity::writeTo(uint8_t*, size_t) { return 0; }
void LocalIdentity::readFrom(const uint8_t*, size_t) {}
void LocalIdentity::sign(uint8_t* sig, const uint8_t*, int) const { memset(sig, 0, SIGNATURE_SIZE); }
void LocalIdentity::calcSharedSecret(uint8_t* secret, const uint8_t*) const { memset(secret, 0, PUB_KEY_SIZE); }

}
