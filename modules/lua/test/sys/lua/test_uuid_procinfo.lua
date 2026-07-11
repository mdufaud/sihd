local sys = sihd.sys

-- Uuid: default ctor generates a random uuid
local u1 = sys.Uuid()
assert(not u1:is_null())
assert(#u1:str() == 36)

local u2 = sys.Uuid()
assert(u1:str() ~= u2:str())

-- from string
local u3 = sys.Uuid("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
assert(u3:str() == "6ba7b810-9dad-11d1-80b4-00c04fd430c8")
assert(tostring(u3) == "6ba7b810-9dad-11d1-80b4-00c04fd430c8")

-- predefined namespaces
assert(sys.Uuid.DNS():str() == "6ba7b810-9dad-11d1-80b4-00c04fd430c8")
assert(sys.Uuid.URL():str() == "6ba7b811-9dad-11d1-80b4-00c04fd430c8")
assert(sys.Uuid.OID():str() == "6ba7b812-9dad-11d1-80b4-00c04fd430c8")
assert(sys.Uuid.X500():str() == "6ba7b814-9dad-11d1-80b4-00c04fd430c8")

-- name-based (v5): same namespace + name = same uuid
local ns = sys.Uuid.DNS()
local u4 = sys.Uuid(ns, "example.com")
local u5 = sys.Uuid(ns, "example.com")
assert(u4:str() == u5:str())
assert(u4 == u5)

local u6 = sys.Uuid(ns, "other.com")
assert(u4:str() ~= u6:str())
assert(not (u4 == u6))

-- clear -> null
local ue = sys.Uuid()
ue:clear()
assert(ue:is_null())

-- ProcessInfo: current process
local os = sihd.sys.os
local self_info = sys.ProcessInfo(os.pid())
assert(self_info:is_alive())
assert(self_info:pid() == os.pid())
assert(#self_info:name() > 0)
assert(type(self_info:cmd_line()) == "table")
assert(type(self_info:env()) == "table")
