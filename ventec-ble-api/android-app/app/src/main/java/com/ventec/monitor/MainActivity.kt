package com.ventec.monitor

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.ventec.ble.VentecLiveState
import com.ventec.monitor.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private lateinit var bleClient: VentecBleClient
    private var lastStatus = ""

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { results ->
        if (results.values.all { it }) {
            beginScanFlow()
        } else {
            Toast.makeText(this, "Permissions Bluetooth + Localisation requises", Toast.LENGTH_LONG).show()
            binding.statusText.text = "Permissions refusées"
        }
    }

    private val enableBluetoothLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) {
        if (bleClient.isBluetoothReady()) beginScanFlow()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        bleClient = VentecBleClient(
            context = this,
            onState = { state, message -> updateConnectionUi(state, message) },
            onLiveUpdate = { state -> updateLiveUi(state) },
        )

        binding.scanButton.setOnClickListener {
            binding.statusText.text = "Préparation du scan…"
            if (hasBlePermissions()) {
                beginScanFlow()
            } else {
                requestBlePermissions()
            }
        }

        binding.disconnectButton.setOnClickListener {
            bleClient.disconnect()
        }
    }

    override fun onDestroy() {
        bleClient.disconnect()
        super.onDestroy()
    }

    private fun beginScanFlow() {
        if (!bleClient.isBluetoothReady()) {
            enableBluetoothLauncher.launch(Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE))
            return
        }
        if (!bleClient.isLocationEnabled()) {
            binding.statusText.text = "Activez la localisation du téléphone"
            Toast.makeText(
                this,
                "BLE nécessite la localisation activée sur ce téléphone",
                Toast.LENGTH_LONG,
            ).show()
            startActivity(Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS))
            return
        }
        bleClient.startScan()
    }

    private fun updateConnectionUi(state: BleConnectionState, message: String) {
        lastStatus = message
        binding.statusText.text = message
        binding.statusText.setTextColor(
            when (state) {
                BleConnectionState.CONNECTED -> Color.parseColor("#2E7D32")
                BleConnectionState.SCANNING, BleConnectionState.CONNECTING -> Color.parseColor("#1565C0")
                BleConnectionState.DISCONNECTED -> Color.parseColor("#C62828")
                else -> Color.parseColor("#424242")
            },
        )
        binding.disconnectButton.isEnabled =
            state == BleConnectionState.CONNECTED ||
            state == BleConnectionState.CONNECTING ||
            state == BleConnectionState.SCANNING
        binding.scanButton.isEnabled =
            state != BleConnectionState.SCANNING && state != BleConnectionState.CONNECTING
        updateLiveUi(binding.dataText.tag as? VentecLiveState ?: VentecLiveState())
    }

    private fun updateLiveUi(state: VentecLiveState) {
        binding.dataText.tag = state
        val body = state.formatDisplay()
        binding.dataText.text = "État : $lastStatus\n\n$body"
    }

    private fun requiredPermissions(): Array<String> {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT,
                Manifest.permission.ACCESS_FINE_LOCATION,
            )
        } else {
            arrayOf(
                Manifest.permission.ACCESS_FINE_LOCATION,
                Manifest.permission.BLUETOOTH,
                Manifest.permission.BLUETOOTH_ADMIN,
            )
        }
    }

    private fun hasBlePermissions(): Boolean =
        requiredPermissions().all {
            ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED
        }

    private fun requestBlePermissions() {
        permissionLauncher.launch(requiredPermissions())
    }
}
