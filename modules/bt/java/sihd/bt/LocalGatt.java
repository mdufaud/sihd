package sihd.bt;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattServer;
import android.bluetooth.BluetoothGattServerCallback;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.content.Context;
import android.os.Build;
import java.io.IOException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.UUID;

// the local GATT server: reads and writes of remote devices go to the native server of handle
final class LocalGatt extends BluetoothGattServerCallback {
    private static final UUID CCCD = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");

    private long handle;
    private BluetoothGattServer server;
    // the devices that enabled notifications or indications, per characteristic
    private final Map<BluetoothGattCharacteristic, Set<BluetoothDevice>> subscribers = new HashMap<>();

    LocalGatt(long handle) {
        this.handle = handle;
    }

    synchronized void open(Context context) throws IOException {
        Adapters.adapter(context);
        BluetoothManager manager = context.getSystemService(BluetoothManager.class);
        server = manager.openGattServer(context, this);
        if (server == null)
            throw new IOException("the system refused a gatt server");
    }

    synchronized void close() {
        handle = 0;
        if (server != null)
            server.close();
        server = null;
        subscribers.clear();
    }

    // completes in nativeOnComplete: the system adds one service at a time
    synchronized void addService(String uuid, boolean primary, String[] uuids, int[] properties) throws IOException {
        int type = primary ? BluetoothGattService.SERVICE_TYPE_PRIMARY : BluetoothGattService.SERVICE_TYPE_SECONDARY;
        BluetoothGattService service = new BluetoothGattService(UUID.fromString(uuid), type);
        for (int i = 0; i < uuids.length; ++i) {
            BluetoothGattCharacteristic characteristic
                = new BluetoothGattCharacteristic(UUID.fromString(uuids[i]), properties[i], permissions(properties[i]));
            if ((properties[i] & (BluetoothGattCharacteristic.PROPERTY_NOTIFY | BluetoothGattCharacteristic.PROPERTY_INDICATE)) != 0) {
                int access = BluetoothGattDescriptor.PERMISSION_READ | BluetoothGattDescriptor.PERMISSION_WRITE;
                characteristic.addDescriptor(new BluetoothGattDescriptor(CCCD, access));
            }
            service.addCharacteristic(characteristic);
        }
        if (!server.addService(service))
            throw new IOException("the system refused the service " + uuid);
    }

    // false once no subscriber is left
    @SuppressWarnings("deprecation")
    synchronized boolean notify(String serviceUuid, String characteristicUuid, byte[] value) {
        BluetoothGattService service = server.getService(UUID.fromString(serviceUuid));
        BluetoothGattCharacteristic characteristic
            = service != null ? service.getCharacteristic(UUID.fromString(characteristicUuid)) : null;
        Set<BluetoothDevice> devices = characteristic != null ? subscribers.get(characteristic) : null;
        if (devices == null || devices.isEmpty())
            return false;
        boolean confirm = (characteristic.getProperties() & BluetoothGattCharacteristic.PROPERTY_NOTIFY) == 0;
        for (BluetoothDevice device : devices) {
            if (Build.VERSION.SDK_INT >= 33) {
                server.notifyCharacteristicChanged(device, characteristic, confirm, value);
            } else {
                characteristic.setValue(value);
                server.notifyCharacteristicChanged(device, characteristic, confirm);
            }
        }
        return true;
    }

    @Override
    public synchronized void onServiceAdded(int status, BluetoothGattService service) {
        if (handle != 0)
            nativeOnComplete(handle, status);
    }

    @Override
    public synchronized void onConnectionStateChange(BluetoothDevice device, int status, int newState) {
        if (newState == BluetoothProfile.STATE_DISCONNECTED) {
            for (Set<BluetoothDevice> devices : subscribers.values())
                devices.remove(device);
        }
    }

    @Override
    public synchronized void onCharacteristicReadRequest(BluetoothDevice device,
                                                         int requestId,
                                                         int offset,
                                                         BluetoothGattCharacteristic characteristic) {
        byte[] value = handle != 0 ? nativeOnRead(handle, uuidOf(characteristic.getService()), characteristic.getUuid().toString()) : null;
        if (value == null)
            server.sendResponse(device, requestId, BluetoothGatt.GATT_FAILURE, offset, null);
        else if (offset > value.length)
            server.sendResponse(device, requestId, BluetoothGatt.GATT_INVALID_OFFSET, offset, null);
        else
            server.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, offset, Arrays.copyOfRange(value, offset, value.length));
    }

    @Override
    public synchronized void onCharacteristicWriteRequest(BluetoothDevice device,
                                                          int requestId,
                                                          BluetoothGattCharacteristic characteristic,
                                                          boolean preparedWrite,
                                                          boolean responseNeeded,
                                                          int offset,
                                                          byte[] value) {
        int status = BluetoothGatt.GATT_REQUEST_NOT_SUPPORTED;
        if (!preparedWrite && offset == 0 && handle != 0) {
            boolean accepted = nativeOnWrite(handle, uuidOf(characteristic.getService()), characteristic.getUuid().toString(), value);
            status = accepted ? BluetoothGatt.GATT_SUCCESS : BluetoothGatt.GATT_WRITE_NOT_PERMITTED;
        }
        if (responseNeeded)
            server.sendResponse(device, requestId, status, offset, null);
    }

    @Override
    public synchronized void onDescriptorReadRequest(BluetoothDevice device,
                                                     int requestId,
                                                     int offset,
                                                     BluetoothGattDescriptor descriptor) {
        Set<BluetoothDevice> devices = subscribers.get(descriptor.getCharacteristic());
        boolean subscribed = devices != null && devices.contains(device);
        byte[] value = subscribed ? BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE : BluetoothGattDescriptor.DISABLE_NOTIFICATION_VALUE;
        server.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, 0, value);
    }

    @Override
    public synchronized void onDescriptorWriteRequest(BluetoothDevice device,
                                                      int requestId,
                                                      BluetoothGattDescriptor descriptor,
                                                      boolean preparedWrite,
                                                      boolean responseNeeded,
                                                      int offset,
                                                      byte[] value) {
        BluetoothGattCharacteristic characteristic = descriptor.getCharacteristic();
        Set<BluetoothDevice> devices = subscribers.get(characteristic);
        if (devices == null) {
            devices = new HashSet<>();
            subscribers.put(characteristic, devices);
        }
        // a CCCD value is two bytes, bit 0 notifications and bit 1 indications
        if (value != null && value.length > 0 && (value[0] & 0x03) != 0)
            devices.add(device);
        else
            devices.remove(device);
        if (responseNeeded)
            server.sendResponse(device, requestId, BluetoothGatt.GATT_SUCCESS, 0, null);
    }

    private static int permissions(int properties) {
        int permissions = 0;
        if ((properties & BluetoothGattCharacteristic.PROPERTY_READ) != 0)
            permissions |= BluetoothGattCharacteristic.PERMISSION_READ;
        int writes = BluetoothGattCharacteristic.PROPERTY_WRITE | BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE;
        if ((properties & writes) != 0)
            permissions |= BluetoothGattCharacteristic.PERMISSION_WRITE;
        return permissions;
    }

    private static String uuidOf(BluetoothGattService service) {
        return service != null ? service.getUuid().toString() : "";
    }

    private static native void nativeOnComplete(long handle, int status);

    // null refuses the read
    private static native byte[] nativeOnRead(long handle, String serviceUuid, String characteristicUuid);

    private static native boolean nativeOnWrite(long handle, String serviceUuid, String characteristicUuid, byte[] value);
}
