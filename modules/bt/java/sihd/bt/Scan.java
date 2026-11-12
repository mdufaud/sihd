package sihd.bt;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothClass;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanRecord;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.ParcelUuid;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

// a BLE scan and a classic discovery reporting to the native scanner of handle
final class Scan extends ScanCallback {
    static final int TRANSPORT_ANY = 0;
    static final int TRANSPORT_LE = 1;
    static final int TRANSPORT_CLASSIC = 2;

    private final Context context;
    private final BluetoothAdapter adapter;
    private long handle;
    private BluetoothLeScanner scanner;
    private BroadcastReceiver receiver;

    Scan(Context context, long handle) throws IOException {
        this.context = context;
        this.adapter = Adapters.adapter(context);
        this.handle = handle;
    }

    synchronized void start(int transport, String[] uuids) throws IOException {
        if (!adapter.isEnabled())
            throw new IOException("bluetooth is off");
        if (transport != TRANSPORT_CLASSIC)
            startLe(uuids);
        if (transport != TRANSPORT_LE)
            startClassic();
    }

    synchronized void stop() {
        handle = 0;
        if (scanner != null && adapter.isEnabled())
            scanner.stopScan(this);
        scanner = null;
        if (receiver != null) {
            context.unregisterReceiver(receiver);
            adapter.cancelDiscovery();
        }
        receiver = null;
    }

    @Override
    public void onScanResult(int callbackType, ScanResult result) {
        report(result);
    }

    @Override
    public void onBatchScanResults(List<ScanResult> results) {
        for (ScanResult result : results)
            report(result);
    }

    @Override
    public synchronized void onScanFailed(int errorCode) {
        if (handle != 0)
            nativeOnScanFailed(handle, errorCode);
    }

    private void startLe(String[] uuids) throws IOException {
        scanner = adapter.getBluetoothLeScanner();
        if (scanner == null)
            throw new IOException("bluetooth is off");
        List<ScanFilter> filters = new ArrayList<>();
        for (String uuid : uuids)
            filters.add(new ScanFilter.Builder().setServiceUuid(ParcelUuid.fromString(uuid)).build());
        ScanSettings settings = new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build();
        scanner.startScan(filters, settings, this);
    }

    private void startClassic() throws IOException {
        receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                onDiscovery(intent);
            }
        };
        IntentFilter filter = new IntentFilter(BluetoothDevice.ACTION_FOUND);
        filter.addAction(BluetoothAdapter.ACTION_DISCOVERY_FINISHED);
        // protected broadcasts of the bluetooth process: a not exported receiver never gets them
        context.registerReceiver(receiver, filter);
        if (!adapter.startDiscovery())
            throw new IOException("the adapter refused a classic discovery");
    }

    private synchronized void report(ScanResult result) {
        if (handle == 0)
            return;
        BluetoothDevice device = result.getDevice();
        ScanRecord record = result.getScanRecord();
        boolean connectable = Build.VERSION.SDK_INT >= 26 && result.isConnectable();
        nativeOnResult(handle,
                       device.getAddress(),
                       addressType(device),
                       "",
                       result.getRssi(),
                       connectable,
                       -1,
                       record != null ? record.getBytes() : null);
    }

    // a classic discovery lasts about 12 seconds: it restarts until stop()
    private synchronized void onDiscovery(Intent intent) {
        if (handle == 0)
            return;
        if (BluetoothAdapter.ACTION_DISCOVERY_FINISHED.equals(intent.getAction())) {
            adapter.startDiscovery();
            return;
        }
        BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
        BluetoothClass bluetoothClass = intent.getParcelableExtra(BluetoothDevice.EXTRA_CLASS);
        String name = intent.getStringExtra(BluetoothDevice.EXTRA_NAME);
        if (device == null)
            return;
        nativeOnResult(handle,
                       device.getAddress(),
                       addressType(device),
                       name != null ? name : "",
                       intent.getShortExtra(BluetoothDevice.EXTRA_RSSI, (short)0),
                       false,
                       bluetoothClass != null ? bluetoothClass.hashCode() : -1,
                       null);
    }

    private static int addressType(BluetoothDevice device) {
        return Build.VERSION.SDK_INT >= 35 ? device.getAddressType() : -1;
    }

    private static native void nativeOnResult(long handle,
                                              String address,
                                              int addressType,
                                              String name,
                                              int rssi,
                                              boolean connectable,
                                              int classOfDevice,
                                              byte[] record);

    private static native void nativeOnScanFailed(long handle, int errorCode);
}
