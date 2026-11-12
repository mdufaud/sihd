package sihd.bt;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothServerSocket;
import android.bluetooth.BluetoothSocket;
import android.content.Context;
import android.os.Build;
import android.os.SystemClock;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.UUID;

// a connected RFCOMM or L2CAP socket, unauthenticated like the sockets of the other platforms: blocking calls of the native side
final class Stream {
    private final BluetoothSocket socket;
    private final InputStream input;
    private final OutputStream output;

    private Stream(BluetoothSocket socket) throws IOException {
        this.socket = socket;
        this.input = socket.getInputStream();
        this.output = socket.getOutputStream();
    }

    static Stream rfcomm(Context context, String address, String uuid) throws IOException {
        BluetoothDevice device = Adapters.device(context, address);
        return connected(device.createInsecureRfcommSocketToServiceRecord(UUID.fromString(uuid)));
    }

    static Stream l2cap(Context context, String address, int psm) throws IOException {
        if (Build.VERSION.SDK_INT < 29)
            throw new UnsupportedOperationException("L2CAP channels need android 10");
        return connected(Adapters.device(context, address).createInsecureL2capChannel(psm));
    }

    String address() {
        return socket.getRemoteDevice().getAddress();
    }

    // 0 once the peer closed
    int read(byte[] buffer) throws IOException {
        int read = input.read(buffer);
        return read < 0 ? 0 : read;
    }

    void write(byte[] data) throws IOException {
        output.write(data);
        output.flush();
    }

    void close() {
        try {
            socket.close();
        } catch (IOException ignored) {
            // closing twice or after a reset fails the same way: the socket is closed either way
        }
    }

    private static Stream connected(BluetoothSocket socket) throws IOException {
        try {
            socket.connect();
            return new Stream(socket);
        } catch (IOException connectionError) {
            socket.close();
            throw connectionError;
        }
    }

    // accepts the connections of an RFCOMM SDP record or of a dynamic L2CAP psm
    static final class Listener {
        private final BluetoothServerSocket server;
        private volatile boolean closed;

        private Listener(BluetoothServerSocket server) {
            this.server = server;
        }

        static Listener rfcomm(Context context, String uuid, String name) throws IOException {
            return new Listener(Adapters.adapter(context).listenUsingInsecureRfcommWithServiceRecord(name, UUID.fromString(uuid)));
        }

        static Listener l2cap(Context context) throws IOException {
            if (Build.VERSION.SDK_INT < 29)
                throw new UnsupportedOperationException("L2CAP channels need android 10");
            return new Listener(Adapters.adapter(context).listenUsingInsecureL2capChannel());
        }

        int psm() {
            return Build.VERSION.SDK_INT >= 29 ? server.getPsm() : 0;
        }

        // null once the timeout elapsed
        Stream accept(int timeoutMilliseconds) throws IOException {
            long start = SystemClock.elapsedRealtime();
            try {
                return new Stream(server.accept(timeoutMilliseconds));
            } catch (IOException acceptError) {
                if (!closed && SystemClock.elapsedRealtime() - start >= timeoutMilliseconds)
                    return null;
                throw acceptError;
            }
        }

        void close() {
            closed = true;
            try {
                server.close();
            } catch (IOException ignored) {
                // the server socket is closed either way
            }
        }
    }
}
