local bt = sihd.bt

backend_reads = {}
backend_writes = {}

working = bt.LocalCharacteristic()
working:set_on_read(function()
    backend_reads[#backend_reads + 1] = 1
    return "\1\2\3"
end)
working:set_on_write(function(value)
    backend_writes[#backend_writes + 1] = value
    return value == "\10\11"
end)

raising = bt.LocalCharacteristic()
raising:set_on_read(function() error("boom") end)
raising:set_on_write(function(value) error("boom") end)

mistyped = bt.LocalCharacteristic()
mistyped:set_on_read(function() return true end)
mistyped:set_on_write(function(value) end)
