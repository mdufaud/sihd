import sihd
channel = sihd.core.Channel("chan", "int", 8)
print(channel.get_name())
root = sihd.util.Node("root", None)
named = sihd.util.Named("child", root)
print(root.get_name())
print(named.get_name())