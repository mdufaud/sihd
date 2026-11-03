package __SBT_NAMESPACE__

import android.app.Activity
import android.content.Intent
import android.net.VpnService
import android.os.Bundle
import android.os.Process

class VpnActivity : Activity() {
    private val consentRequest = 1

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // a recreated activity still receives the pending consent result
        if (savedInstanceState != null) {
            return
        }
        // the terminal runs main() once per process: a new capture needs a new process
        if (TerminalActivity.nativeMainCalled) {
            if (CaptureVpnService.capturing()) {
                startActivity(terminalIntent().addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT))
                finish()
            } else {
                startActivity(intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK))
                Process.killProcess(Process.myPid())
            }
            return
        }
        val consent = VpnService.prepare(this)
        if (consent != null) {
            startActivityForResult(consent, consentRequest)
        } else {
            startCapture()
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == consentRequest && resultCode == RESULT_OK) {
            startCapture()
        } else {
            finish()
        }
    }

    private fun startCapture() {
        CaptureVpnService.onEstablished = { args ->
            CaptureVpnService.onEstablished = null
            if (args != null) {
                startActivity(terminalIntent().addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK).putExtra("restart_args", args))
            }
            finish()
        }
        startService(Intent(this, CaptureVpnService::class.java))
    }

    private fun terminalIntent() = Intent(this, TerminalActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
}
