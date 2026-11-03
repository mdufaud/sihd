package __SBT_NAMESPACE__

import android.content.Intent
import android.net.ConnectivityManager
import android.net.VpnService
import java.io.File

class CaptureVpnService : VpnService() {
    companion object {
        // the activity in foreground launches the terminal: android refuses a launch from this service
        var onEstablished: ((String?) -> Unit)? = null
        private var tunFd = -1

        // the native capture closes the detached descriptor when it ends
        fun capturing() = tunFd >= 0 && File("/proc/self/fd/$tunFd").canonicalPath == "/dev/tun"
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        // a second establish() moves the VPN to a new interface, away from the one the capture reads
        if (capturing()) {
            onEstablished?.invoke(null)
            return START_NOT_STICKY
        }
        val builder = Builder()
            .setSession("sihd capture")
            .setMtu(1500)
            .addAddress("10.215.173.1", 32)
            .addAddress("fd00:215:173::1", 128)
            .addRoute("0.0.0.0", 0)
            .addRoute("::", 0)
            // the relay sockets must not loop back into the tun: this app bypasses its own VPN
            .addDisallowedApplication(packageName)
        val connectivity = getSystemService(ConnectivityManager::class.java)
        connectivity.getLinkProperties(connectivity.activeNetwork)?.dnsServers?.forEach {
            builder.addDnsServer(it)
        }
        val tun = builder.establish()
        if (tun == null) {
            onEstablished?.invoke(null)
            stopSelf()
            return START_NOT_STICKY
        }
        val output = filesDir.absolutePath + "/capture.pcap"
        tunFd = tun.detachFd()
        onEstablished?.invoke("--tun-fd $tunFd --output $output")
        return START_NOT_STICKY
    }
}
