package com.ventec.monitor

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.location.LocationManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import com.ventec.ble.VentecBleParser
import com.ventec.ble.VentecBleUuids
import com.ventec.ble.VentecLiveState
import java.util.ArrayDeque
import java.util.UUID

enum class BleConnectionState {
    IDLE, SCANNING, CONNECTING, CONNECTED, DISCONNECTED
}

class VentecBleClient(
    private val context: Context,
    private val onState: (BleConnectionState, String) -> Unit,
    private val onLiveUpdate: (VentecLiveState) -> Unit,
) {
    private val mainHandler = Handler(Looper.getMainLooper())
    private val adapter: BluetoothAdapter? =
        (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter

    private var gatt: BluetoothGatt? = null
    private val liveState = VentecLiveState()
    private val notifyQueue = ArrayDeque<BluetoothGattCharacteristic>()
    private val readQueue = ArrayDeque<BluetoothGattCharacteristic>()
    private var isEnablingNotify = false
    private var devicesSeen = 0

    private val scanTimeout = Runnable {
        stopScan()
        setState(
            BleConnectionState.IDLE,
            "Carte introuvable ($devicesSeen périph. vus). Alimentation + firmware v1.3 ?",
        )
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            devicesSeen++
            if (!isVentecDevice(result)) return
            mainHandler.removeCallbacks(scanTimeout)
            stopScan()
            val name = result.scanRecord?.deviceName ?: result.device.name ?: "Ventec"
            setState(BleConnectionState.CONNECTING, "Carte trouvée : $name")
            connect(result.device)
        }

        override fun onScanFailed(errorCode: Int) {
            mainHandler.removeCallbacks(scanTimeout)
            setState(BleConnectionState.IDLE, "Scan BLE échoué (code $errorCode)")
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS && newState != BluetoothProfile.STATE_CONNECTED) {
                setState(BleConnectionState.DISCONNECTED, "Connexion échouée (code $status)")
                cleanupGatt()
                return
            }
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    setState(BleConnectionState.CONNECTING, "Connecté — découverte des services…")
                    gatt.requestMtu(512)
                    gatt.discoverServices()
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    setState(BleConnectionState.DISCONNECTED, "Déconnecté")
                    cleanupGatt()
                }
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                setState(BleConnectionState.DISCONNECTED, "Services introuvables")
                disconnect()
                return
            }
            val service = gatt.getService(VentecBleUuids.SERVICE)
            if (service == null) {
                setState(
                    BleConnectionState.DISCONNECTED,
                    "Service Ventec absent — reflasher firmware v1.3+",
                )
                disconnect()
                return
            }
            notifyQueue.clear()
            readQueue.clear()
            VentecBleUuids.NOTIFY_CHARACTERISTICS.forEach { uuid ->
                service.getCharacteristic(uuid)?.let {
                    notifyQueue.add(it)
                    readQueue.add(it)
                }
            }
            if (notifyQueue.isEmpty()) {
                setState(BleConnectionState.DISCONNECTED, "Caractéristiques BLE introuvables")
                disconnect()
                return
            }
            setState(BleConnectionState.CONNECTING, "Activation des notifications BLE…")
            isEnablingNotify = true
            enableNextNotification(gatt)
        }

        override fun onDescriptorWrite(
            gatt: BluetoothGatt,
            descriptor: BluetoothGattDescriptor,
            status: Int,
        ) {
            if (!isEnablingNotify) return
            if (status != BluetoothGatt.GATT_SUCCESS) {
                setState(BleConnectionState.DISCONNECTED, "Notifications refusées (code $status)")
                disconnect()
                return
            }
            enableNextNotification(gatt)
        }

        override fun onCharacteristicRead(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray,
            status: Int,
        ) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                handlePayload(characteristic.uuid, value)
            }
            readNextCharacteristic(gatt)
        }

        @Deprecated("Compat API < 33")
        override fun onCharacteristicRead(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int,
        ) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                @Suppress("DEPRECATION")
                handlePayload(characteristic.uuid, characteristic.value ?: byteArrayOf())
            }
            readNextCharacteristic(gatt)
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray,
        ) {
            handlePayload(characteristic.uuid, value)
        }

        @Deprecated("Compat API < 33")
        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
        ) {
            @Suppress("DEPRECATION")
            handlePayload(characteristic.uuid, characteristic.value ?: return)
        }
    }

    fun isBluetoothReady(): Boolean = adapter?.isEnabled == true

    fun isLocationEnabled(): Boolean {
        val lm = context.getSystemService(Context.LOCATION_SERVICE) as LocationManager
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            lm.isLocationEnabled
        } else {
            @Suppress("DEPRECATION")
            lm.isProviderEnabled(LocationManager.GPS_PROVIDER) ||
                lm.isProviderEnabled(LocationManager.NETWORK_PROVIDER)
        }
    }

    @SuppressLint("MissingPermission")
    fun startScan() {
        if (adapter == null || !adapter.isEnabled) {
            setState(BleConnectionState.IDLE, "Activez le Bluetooth")
            return
        }
        if (!isLocationEnabled()) {
            setState(
                BleConnectionState.IDLE,
                "Activez la LOCALISATION (Paramètres → Localisation)",
            )
            return
        }

        stopScan()
        devicesSeen = 0
        resetLiveState()

        setState(BleConnectionState.SCANNING, "Scan en cours… cherchez Ventec-AGV-Monitor")
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()
        // Scan sans filtre : plus fiable sur Xiaomi / Samsung
        adapter.bluetoothLeScanner?.startScan(null, settings, scanCallback)
            ?: setState(BleConnectionState.IDLE, "Scanner BLE indisponible")

        mainHandler.postDelayed(scanTimeout, SCAN_TIMEOUT_MS)
    }

    @SuppressLint("MissingPermission")
    fun stopScan() {
        mainHandler.removeCallbacks(scanTimeout)
        adapter?.bluetoothLeScanner?.stopScan(scanCallback)
    }

    @SuppressLint("MissingPermission")
    fun disconnect() {
        stopScan()
        gatt?.disconnect()
        cleanupGatt()
        setState(BleConnectionState.DISCONNECTED, "Déconnecté")
    }

    @SuppressLint("MissingPermission")
    private fun connect(device: BluetoothDevice) {
        gatt?.close()
        gatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
    }

    @SuppressLint("MissingPermission")
    private fun cleanupGatt() {
        isEnablingNotify = false
        notifyQueue.clear()
        readQueue.clear()
        gatt?.close()
        gatt = null
    }

    @SuppressLint("MissingPermission")
    private fun enableNextNotification(gatt: BluetoothGatt) {
        val next = notifyQueue.poll()
        if (next == null) {
            isEnablingNotify = false
            setState(BleConnectionState.CONNECTED, "Connecté — réception des données")
            readNextCharacteristic(gatt)
            return
        }
        gatt.setCharacteristicNotification(next, true)
        val cccd = next.getDescriptor(CCCD_UUID)
        if (cccd == null) {
            enableNextNotification(gatt)
            return
        }
        val ok = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            gatt.writeDescriptor(cccd, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
        } else {
            @Suppress("DEPRECATION")
            cccd.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
            @Suppress("DEPRECATION")
            gatt.writeDescriptor(cccd)
        }
        if (!ok) {
            setState(BleConnectionState.DISCONNECTED, "Erreur activation notifications")
            disconnect()
        }
    }

    @SuppressLint("MissingPermission")
    private fun readNextCharacteristic(gatt: BluetoothGatt) {
        val next = readQueue.poll() ?: return
        if (!gatt.readCharacteristic(next)) {
            readNextCharacteristic(gatt)
        }
    }

    private fun resetLiveState() {
        liveState.apply {
            currentA = null
            temps = null
            alarmLevel = null
            status = null
            telemetry = null
            lastAlarmEvent = null
            lastUpdateMs = 0L
        }
        mainHandler.post { onLiveUpdate(liveState) }
    }

    private fun handlePayload(uuid: UUID, value: ByteArray) {
        if (value.isEmpty()) return
        when (uuid) {
            VentecBleUuids.CHAR_CURRENT ->
                VentecBleParser.parseCurrent(value)?.let { liveState.currentA = it }
            VentecBleUuids.CHAR_TEMPS ->
                VentecBleParser.parseTemps(value)?.let { liveState.temps = it }
            VentecBleUuids.CHAR_ALARM ->
                VentecBleParser.parseAlarmLevel(value)?.let { liveState.alarmLevel = it }
            VentecBleUuids.CHAR_STATUS ->
                VentecBleParser.parseStatus(value)?.let { liveState.status = it }
            VentecBleUuids.CHAR_TELEMETRY ->
                VentecBleParser.parseTelemetry(value)?.let { liveState.telemetry = it }
            VentecBleUuids.CHAR_ALARM_EVENT ->
                VentecBleParser.parseAlarmEvent(value)?.let { liveState.lastAlarmEvent = it }
        }
        liveState.lastUpdateMs = System.currentTimeMillis()
        mainHandler.post { onLiveUpdate(liveState) }
    }

    private fun isVentecDevice(result: ScanResult): Boolean {
        val name = result.scanRecord?.deviceName ?: result.device.name
        if (!name.isNullOrBlank()) {
            if (name.equals(VentecBleUuids.DEVICE_NAME, ignoreCase = true)) return true
            if (name.contains("Ventec", ignoreCase = true)) return true
        }
        val serviceUuids = result.scanRecord?.serviceUuids ?: return false
        return serviceUuids.any { it.uuid == VentecBleUuids.SERVICE }
    }

    private fun setState(state: BleConnectionState, message: String) {
        mainHandler.post { onState(state, message) }
    }

    companion object {
        private const val SCAN_TIMEOUT_MS = 15_000L
        private val CCCD_UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    }
}
