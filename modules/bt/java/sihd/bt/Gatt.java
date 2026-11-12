package sihd.bt;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothProfile;
import android.content.Context;
import android.os.Build;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;

// a GATT client connection: the native side runs one operation at a time and waits for its nativeOnComplete
final class Gatt extends BluetoothGattCallback {
    private static final UUID CCCD = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");

    private long handle;
    private BluetoothGatt gatt;
    private int mtu = 23;
    private final Map<Integer, BluetoothGattCharacteristic> characteristics = new HashMap<>();
    private final Map<Integer, BluetoothGattDescriptor> descriptors = new HashMap<>();

    Gatt(long handle) {
        this.handle = handle;
    }

    synchronized void connect(Context context, String address) throws IOException {
        BluetoothDevice device = Adapters.device(context, address);
        gatt = device.connectGatt(context, false, this, BluetoothDevice.TRANSPORT_LE);
        if (gatt == null)
            throw new IOException("the system refused a connection to " + address);
    }

    synchronized void close() {
        handle = 0;
        if (gatt != null) {
            gatt.disconnect();
            gatt.close();
        }
        gatt = null;
    }

    synchronized void discover() throws IOException {
        check(gatt.discoverServices(), "service discovery");
    }

    synchronized void requestMtu(int wanted) throws IOException {
        check(gatt.requestMtu(wanted), "mtu request");
    }

    synchronized int mtu() {
        return mtu;
    }

    // "s uuid handle primary", "c uuid handle properties", "d uuid handle": the tree in discovery order
    synchronized String[] attributes() {
        characteristics.clear();
        descriptors.clear();
        List<String> lines = new ArrayList<>();
        for (BluetoothGattService service : gatt.getServices()) {
            boolean primary = service.getType() == BluetoothGattService.SERVICE_TYPE_PRIMARY;
            lines.add("s " + service.getUuid() + " " + service.getInstanceId() + " " + (primary ? 1 : 0));
            for (BluetoothGattCharacteristic characteristic : service.getCharacteristics()) {
                int instance = characteristic.getInstanceId();
                characteristics.put(instance, characteristic);
                lines.add("c " + characteristic.getUuid() + " " + instance + " " + characteristic.getProperties());
                // descriptors follow the characteristic value handle: android does not expose their handles
                int offset = 1;
                for (BluetoothGattDescriptor descriptor : characteristic.getDescriptors()) {
                    descriptors.put(instance + offset, descriptor);
                    lines.add("d " + descriptor.getUuid() + " " + (instance + offset));
                    ++offset;
                }
            }
        }
        return lines.toArray(new String[0]);
    }

    synchronized void read(int instance) throws IOException {
        check(gatt.readCharacteristic(characteristic(instance)), "read");
    }

    @SuppressWarnings("deprecation")
    synchronized void write(int instance, byte[] value, boolean withResponse) throws IOException {
        BluetoothGattCharacteristic characteristic = characteristic(instance);
        int type = withResponse ? BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT : BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE;
        if (Build.VERSION.SDK_INT >= 33) {
            check(gatt.writeCharacteristic(characteristic, value, type) == 0, "write");
            return;
        }
        characteristic.setWriteType(type);
        characteristic.setValue(value);
        check(gatt.writeCharacteristic(characteristic), "write");
    }

    synchronized void readDescriptor(int instance) throws IOException {
        check(gatt.readDescriptor(descriptor(instance)), "descriptor read");
    }

    synchronized void writeDescriptor(int instance, byte[] value) throws IOException {
        writeDescriptor(descriptor(instance), value);
    }

    // the CCCD write completes like any descriptor write
    synchronized void subscribe(int instance, boolean enable) throws IOException {
        BluetoothGattCharacteristic characteristic = characteristic(instance);
        BluetoothGattDescriptor cccd = characteristic.getDescriptor(CCCD);
        if (cccd == null)
            throw new IllegalArgumentException("characteristic " + characteristic.getUuid() + " has no client configuration");
        check(gatt.setCharacteristicNotification(characteristic, enable), "notification setting");
        boolean notifies = (characteristic.getProperties() & BluetoothGattCharacteristic.PROPERTY_NOTIFY) != 0;
        byte[] value = !enable ? BluetoothGattDescriptor.DISABLE_NOTIFICATION_VALUE
                       : notifies ? BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                                  : BluetoothGattDescriptor.ENABLE_INDICATION_VALUE;
        writeDescriptor(cccd, value);
    }

    @Override
    public synchronized void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
        if (handle != 0)
            nativeOnConnection(handle, newState == BluetoothProfile.STATE_CONNECTED, status);
    }

    @Override
    public synchronized void onServicesDiscovered(BluetoothGatt gatt, int status) {
        complete(status, null);
    }

    @Override
    public synchronized void onMtuChanged(BluetoothGatt gatt, int mtu, int status) {
        if (status == BluetoothGatt.GATT_SUCCESS)
            this.mtu = mtu;
        complete(status, null);
    }

    @Override
    @SuppressWarnings("deprecation")
    public synchronized void onCharacteristicRead(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
        complete(status, characteristic.getValue());
    }

    @Override
    public synchronized void onCharacteristicRead(BluetoothGatt gatt,
                                                  BluetoothGattCharacteristic characteristic,
                                                  byte[] value,
                                                  int status) {
        complete(status, value);
    }

    @Override
    public synchronized void onCharacteristicWrite(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
        complete(status, null);
    }

    @Override
    @SuppressWarnings("deprecation")
    public synchronized void onDescriptorRead(BluetoothGatt gatt, BluetoothGattDescriptor descriptor, int status) {
        complete(status, descriptor.getValue());
    }

    @Override
    public synchronized void onDescriptorRead(BluetoothGatt gatt, BluetoothGattDescriptor descriptor, int status, byte[] value) {
        complete(status, value);
    }

    @Override
    public synchronized void onDescriptorWrite(BluetoothGatt gatt, BluetoothGattDescriptor descriptor, int status) {
        complete(status, null);
    }

    @Override
    @SuppressWarnings("deprecation")
    public synchronized void onCharacteristicChanged(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic) {
        notified(characteristic, characteristic.getValue());
    }

    @Override
    public synchronized void onCharacteristicChanged(BluetoothGatt gatt,
                                                     BluetoothGattCharacteristic characteristic,
                                                     byte[] value) {
        notified(characteristic, value);
    }

    private BluetoothGattCharacteristic characteristic(int instance) {
        BluetoothGattCharacteristic characteristic = characteristics.get(instance);
        if (characteristic == null)
            throw new IllegalArgumentException("no characteristic of handle " + instance);
        return characteristic;
    }

    private BluetoothGattDescriptor descriptor(int instance) {
        BluetoothGattDescriptor descriptor = descriptors.get(instance);
        if (descriptor == null)
            throw new IllegalArgumentException("no descriptor of handle " + instance);
        return descriptor;
    }

    @SuppressWarnings("deprecation")
    private void writeDescriptor(BluetoothGattDescriptor descriptor, byte[] value) throws IOException {
        if (Build.VERSION.SDK_INT >= 33) {
            check(gatt.writeDescriptor(descriptor, value) == 0, "descriptor write");
            return;
        }
        descriptor.setValue(value);
        check(gatt.writeDescriptor(descriptor), "descriptor write");
    }

    private void complete(int status, byte[] value) {
        if (handle != 0)
            nativeOnComplete(handle, status, value);
    }

    private void notified(BluetoothGattCharacteristic characteristic, byte[] value) {
        if (handle != 0)
            nativeOnNotification(handle, characteristic.getInstanceId(), value);
    }

    private static void check(boolean started, String operation) throws IOException {
        if (!started)
            throw new IOException("the system refused the " + operation);
    }

    private static native void nativeOnConnection(long handle, boolean connected, int status);

    private static native void nativeOnComplete(long handle, int status, byte[] value);

    private static native void nativeOnNotification(long handle, int instance, byte[] value);
}
