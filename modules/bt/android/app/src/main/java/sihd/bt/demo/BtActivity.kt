package __SBT_NAMESPACE__

import android.Manifest
import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.widget.Toast

// asks the bluetooth permissions, hands the context to sihd::bt, then runs the demo in the terminal
class BtActivity : Activity() {
    companion object {
        private const val permissionsRequest = 1
        private const val enableRequest = 2

        init {
            System.loadLibrary("__SBT_LIB_NAME__")
        }

        // empty once sihd::bt holds the context, else why it does not
        @JvmStatic
        private external fun nativeSetContext(context: Context): String
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (savedInstanceState != null) {
            return
        }
        // main() runs once per process: the terminal shows the running demo again
        if (TerminalActivity.nativeMainCalled) {
            startTerminal()
            return
        }
        val missing = permissions().filter { checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }
        if (missing.isEmpty()) {
            enable()
        } else {
            requestPermissions(missing.toTypedArray(), permissionsRequest)
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
        if (requestCode == permissionsRequest) {
            enable()
        }
    }

    @Deprecated("the activity result api needs androidx")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (requestCode == enableRequest) {
            startTerminal()
        }
    }

    private fun permissions(): List<String> =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            listOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT,
                Manifest.permission.BLUETOOTH_ADVERTISE
            )
        } else {
            listOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }

    // only the user turns bluetooth on, through this system dialog
    @Suppress("DEPRECATION")
    private fun enable() {
        val adapter = getSystemService(BluetoothManager::class.java)?.adapter
        val connect = Build.VERSION.SDK_INT < Build.VERSION_CODES.S ||
            checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED
        if (adapter != null && connect && !adapter.isEnabled) {
            startActivityForResult(Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE), enableRequest)
        } else {
            startTerminal()
        }
    }

    private fun startTerminal() {
        if (!TerminalActivity.nativeMainCalled) {
            val error = nativeSetContext(applicationContext)
            if (error.isNotEmpty()) {
                Toast.makeText(applicationContext, error, Toast.LENGTH_LONG).show()
            }
        }
        startActivity(
            Intent(this, TerminalActivity::class.java)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
        )
        finish()
    }
}
