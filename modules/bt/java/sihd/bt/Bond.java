package sihd.bt;

import android.bluetooth.BluetoothDevice;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import java.io.IOException;

// a pairing through the system dialog: the bond state changes go to nativeOnBond
final class Bond extends BroadcastReceiver {
    private final Context context;
    private final BluetoothDevice device;
    private long handle;
    private boolean registered;

    Bond(Context context, String address, long handle) throws IOException {
        this.context = context;
        this.device = Adapters.device(context, address);
        this.handle = handle;
    }

    // false when the device is already bonded
    synchronized boolean start() throws IOException {
        if (device.getBondState() == BluetoothDevice.BOND_BONDED)
            return false;
        IntentFilter filter = new IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
        // protected broadcasts of the bluetooth process: a not exported receiver never gets them
        context.registerReceiver(this, filter);
        registered = true;
        if (!device.createBond())
            throw new IOException("the system refused to pair " + device.getAddress());
        return true;
    }

    synchronized void stop() {
        handle = 0;
        if (registered)
            context.unregisterReceiver(this);
        registered = false;
    }

    @Override
    public synchronized void onReceive(Context context, Intent intent) {
        BluetoothDevice changed = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
        if (handle == 0 || changed == null || !changed.getAddress().equals(device.getAddress()))
            return;
        nativeOnBond(handle, intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE, BluetoothDevice.BOND_NONE));
    }

    private static native void nativeOnBond(long handle, int state);
}
