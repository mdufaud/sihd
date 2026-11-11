package __SBT_NAMESPACE__

import android.app.Activity
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbDeviceConnection
import android.hardware.usb.UsbManager
import android.os.Build
import android.os.Bundle
import android.os.Process

class UsbActivity : Activity() {
    companion object {
        // the native program reads the descriptor of this connection: it must stay open
        private var connection: UsbDeviceConnection? = null
    }

    private val permissionAction by lazy { "$packageName.USB_PERMISSION" }

    private val permissionReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            unregisterReceiver(this)
            val manager = getSystemService(UsbManager::class.java)
            val device = manager.deviceList.values.firstOrNull()
            if (device != null && manager.hasPermission(device)) {
                startTerminal(open(manager, device))
            } else {
                startTerminal("")
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (savedInstanceState != null) {
            return
        }
        // the terminal runs main() once per process: a new device needs a new process
        if (TerminalActivity.nativeMainCalled) {
            startActivity(intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK))
            Process.killProcess(Process.myPid())
            return
        }
        val manager = getSystemService(UsbManager::class.java)
        val device = manager.deviceList.values.firstOrNull()
        when {
            device == null -> startTerminal("")
            manager.hasPermission(device) -> startTerminal(open(manager, device))
            else -> requestPermission(manager, device)
        }
    }

    private fun requestPermission(manager: UsbManager, device: UsbDevice) {
        val filter = IntentFilter(permissionAction)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(permissionReceiver, filter, Context.RECEIVER_NOT_EXPORTED)
        } else {
            registerReceiver(permissionReceiver, filter)
        }
        // mutable: the system adds the device and the answer to the intent
        val flags = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) PendingIntent.FLAG_MUTABLE else 0
        val request = PendingIntent.getBroadcast(this, 0, Intent(permissionAction).setPackage(packageName), flags)
        manager.requestPermission(device, request)
    }

    private fun open(manager: UsbManager, device: UsbDevice): String {
        val opened = manager.openDevice(device) ?: return ""
        connection = opened
        return "--usb-fd ${opened.fileDescriptor}"
    }

    private fun startTerminal(args: String) {
        startActivity(
            Intent(this, TerminalActivity::class.java)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
                .putExtra("restart_args", args)
        )
        finish()
    }
}
