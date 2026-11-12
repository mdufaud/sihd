package sihd.bt;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothClass;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.content.Context;
import android.os.Build;
import android.os.ParcelUuid;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

// the adapter and the remote devices the system knows: static calls of the native backend
final class Adapters {
    private Adapters() {}

    static BluetoothAdapter adapter(Context context) throws IOException {
        BluetoothManager manager = context.getSystemService(BluetoothManager.class);
        BluetoothAdapter adapter = manager != null ? manager.getAdapter() : null;
        if (adapter == null)
            throw new UnsupportedOperationException("this device has no bluetooth adapter");
        return adapter;
    }

    static BluetoothDevice device(Context context, String address) throws IOException {
        return adapter(context).getRemoteDevice(address);
    }

    // address, name, enabled, discoverable, discovering
    static String[] adapterInfo(Context context) throws IOException {
        BluetoothAdapter adapter = adapter(context);
        boolean discoverable = adapter.getScanMode() == BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE;
        return new String[] {
            adapter.getAddress(),
            adapter.getName(),
            flag(adapter.isEnabled()),
            flag(discoverable),
            flag(adapter.isDiscovering()),
        };
    }

    static void setName(Context context, String name) throws IOException {
        if (!adapter(context).setName(name))
            throw new IOException("the adapter refused the name " + name);
    }

    // name, address type (-1 unknown), bonded, connected, class of device (-1 unknown), then the cached uuids
    static String[] deviceInfo(Context context, String address) throws IOException {
        BluetoothDevice device = device(context, address);
        BluetoothManager manager = context.getSystemService(BluetoothManager.class);
        boolean connected = manager.getConnectionState(device, BluetoothProfile.GATT) == BluetoothProfile.STATE_CONNECTED;
        BluetoothClass bluetoothClass = device.getBluetoothClass();
        List<String> info = new ArrayList<>();
        info.add(device.getName() != null ? device.getName() : "");
        info.add(Integer.toString(Build.VERSION.SDK_INT >= 35 ? device.getAddressType() : -1));
        info.add(flag(device.getBondState() == BluetoothDevice.BOND_BONDED));
        info.add(flag(connected));
        // hashCode() is the whole class of device field
        info.add(Integer.toString(bluetoothClass != null ? bluetoothClass.hashCode() : -1));
        ParcelUuid[] uuids = device.getUuids();
        if (uuids != null) {
            for (ParcelUuid uuid : uuids)
                info.add(uuid.toString());
        }
        return info.toArray(new String[0]);
    }

    private static String flag(boolean value) {
        return value ? "1" : "0";
    }
}
