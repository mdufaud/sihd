import sihd

Uuid = sihd.sys.Uuid

# test default constructor - generates random UUID
u1 = Uuid()
assert(not u1.is_null())
assert(len(str(u1)) == 36)  # UUID format: 8-4-4-4-12

# test another random UUID is different
u2 = Uuid()
assert(not u2.is_null())
assert(str(u1) != str(u2))

# test construction from string
u3 = Uuid("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
assert(not u3.is_null())
assert(str(u3) == "6ba7b810-9dad-11d1-80b4-00c04fd430c8")

# test predefined namespaces
dns = Uuid.DNS()
assert(str(dns) == "6ba7b810-9dad-11d1-80b4-00c04fd430c8")

url = Uuid.URL()
assert(str(url) == "6ba7b811-9dad-11d1-80b4-00c04fd430c8")

oid = Uuid.OID()
assert(str(oid) == "6ba7b812-9dad-11d1-80b4-00c04fd430c8")

x500 = Uuid.X500()
assert(str(x500) == "6ba7b814-9dad-11d1-80b4-00c04fd430c8")

# test name-based UUID (UUID v5)
ns = Uuid.DNS()
u4 = Uuid(ns, "example.com")
u5 = Uuid(ns, "example.com")
assert(str(u4) == str(u5))  # same namespace + name = same UUID

u6 = Uuid(ns, "other.com")
assert(str(u4) != str(u6))  # different name = different UUID

# test equality operator
assert(u4 == u5)
assert(not (u4 == u6))

# test bool conversion
assert(bool(u1))
u_empty = Uuid()
u_empty.clear()
assert(not bool(u_empty))
assert(u_empty.is_null())

# test repr
assert(repr(u1) == str(u1))

print("uuid tests passed")
