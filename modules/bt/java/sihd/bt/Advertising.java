package sihd.bt;

import android.bluetooth.le.AdvertiseCallback;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertiseSettings;
import android.bluetooth.le.BluetoothLeAdvertiser;
import android.content.Context;
import android.os.ParcelUuid;
import java.io.IOException;

// a BLE advertisement: its start completes in nativeOnComplete
final class Advertising extends AdvertiseCallback {
    private long handle;
    private BluetoothLeAdvertiser advertiser;

    Advertising(long handle) {
        this.handle = handle;
    }

    synchronized void start(Context context,
                            boolean connectable,
                            String[] serviceUuids,
                            int[] manufacturerIds,
                            byte[][] manufacturerData,
                            String[] serviceDataUuids,
                            byte[][] serviceData) throws IOException {
        advertiser = Adapters.adapter(context).getBluetoothLeAdvertiser();
        if (advertiser == null)
            throw new IOException("bluetooth is off or this adapter cannot advertise");
        AdvertiseSettings settings = new AdvertiseSettings.Builder()
                                         .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
                                         .setConnectable(connectable)
                                         .setTimeout(0)
                                         .build();
        AdvertiseData.Builder data = new AdvertiseData.Builder().setIncludeDeviceName(false);
        for (String uuid : serviceUuids)
            data.addServiceUuid(ParcelUuid.fromString(uuid));
        for (int i = 0; i < manufacturerIds.length; ++i)
            data.addManufacturerData(manufacturerIds[i], manufacturerData[i]);
        for (int i = 0; i < serviceDataUuids.length; ++i)
            data.addServiceData(ParcelUuid.fromString(serviceDataUuids[i]), serviceData[i]);
        advertiser.startAdvertising(settings, data.build(), this);
    }

    synchronized void stop() {
        handle = 0;
        if (advertiser != null)
            advertiser.stopAdvertising(this);
        advertiser = null;
    }

    @Override
    public synchronized void onStartSuccess(AdvertiseSettings settingsInEffect) {
        if (handle != 0)
            nativeOnComplete(handle, 0);
    }

    @Override
    public synchronized void onStartFailure(int errorCode) {
        if (handle != 0)
            nativeOnComplete(handle, errorCode);
    }

    private static native void nativeOnComplete(long handle, int status);
}
