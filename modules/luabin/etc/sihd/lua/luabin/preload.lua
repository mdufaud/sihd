table_print = function(tbl)
    if type(tbl) ~= "table" then
        return
    end
    for key, value in pairs(tbl) do
        print(tostring(key) .. ":", value)
    end
end
