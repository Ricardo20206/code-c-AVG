package com.ventec.ble

import java.nio.ByteBuffer
import java.nio.ByteOrder

data class VentecTemps(val pcb1: Float, val pcb2: Float, val ambient: Float)

data class VentecStatus(
    val powerSource: Int,
    val vibrationPresent: Boolean,
    val humidityPresent: Boolean,
    val loraPresent: Boolean,
)

data class VentecTelemetry(
    val humidityPct: Float,
    val vibrationMg: Float,
    val inaOk: Boolean,
    val tmpOk: Boolean,
    val vibrationModule: Boolean,
    val humidityModule: Boolean,
    val loraModule: Boolean,
    val labActive: Boolean,
)

data class VentecAlarmEvent(
    val timestampMs: Long,
    val type: Int,
    val level: Int,
    val triggerValue: Float,
    val threshold: Float,
)

data class VentecThresholds(
    val warnA: Float,
    val alarmA: Float,
    val tempC: Float,
    val driftA: Float,
)

data class VentecLiveState(
    var currentA: Float? = null,
    var temps: VentecTemps? = null,
    var alarmLevel: Int? = null,
    var status: VentecStatus? = null,
    var telemetry: VentecTelemetry? = null,
    var lastAlarmEvent: VentecAlarmEvent? = null,
    var lastUpdateMs: Long = 0L,
) {
    fun formatDisplay(): String {
        val lines = mutableListOf<String>()
        currentA?.let { lines += "Courant 36V      : ${"%.2f".format(it)} A" }
        temps?.let {
            lines += "Temp. PCB1       : ${"%.1f".format(it.pcb1)} °C"
            lines += "Temp. PCB2       : ${"%.1f".format(it.pcb2)} °C"
            lines += "Temp. ambiante   : ${"%.1f".format(it.ambient)} °C"
        }
        alarmLevel?.let { lines += "Alarme           : ${alarmLabel(it)}" }
        status?.let {
            lines += "Source alim.     : ${powerLabel(it.powerSource)}"
            lines += "Grove vibration  : ${present(it.vibrationPresent)}"
            lines += "Grove humidité   : ${present(it.humidityPresent)}"
            lines += "LoRa MikroBus    : ${present(it.loraPresent)}"
        }
        telemetry?.let {
            if (it.humidityPct >= 0f) {
                lines += "Humidité         : ${"%.1f".format(it.humidityPct)} %"
            } else {
                lines += "Humidité         : N/A"
            }
            if (it.vibrationMg >= 0f) {
                lines += "Vibration        : ${"%.0f".format(it.vibrationMg)} mg"
            } else {
                lines += "Vibration        : N/A"
            }
            lines += "INA237           : ${ok(it.inaOk)}"
            lines += "TMP126           : ${ok(it.tmpOk)}"
            if (it.labActive) lines += "Mode             : LABO (simulation)"
        }
        lastAlarmEvent?.let {
            lines += ""
            lines += "--- Dernier événement alarme ---"
            lines += "Type             : ${alarmTypeLabel(it.type)}"
            lines += "Niveau           : ${alarmLabel(it.level)}"
            lines += "Valeur           : ${"%.2f".format(it.triggerValue)}"
            lines += "Seuil            : ${"%.2f".format(it.threshold)}"
        }
        if (lastUpdateMs > 0L) {
            lines += ""
            lines += "Maj. ${android.text.format.DateFormat.format("HH:mm:ss", lastUpdateMs)}"
        }
        return if (lines.isEmpty()) "En attente de données…" else lines.joinToString("\n")
    }

    private fun alarmLabel(level: Int): String = when (level) {
        1 -> "WARNING"
        2 -> "CRITIQUE"
        else -> "OK"
    }

    private fun alarmTypeLabel(type: Int): String = when (type) {
        1 -> "Courant élevé"
        2 -> "Température PCB"
        3 -> "Dérive courant"
        else -> "Inconnu ($type)"
    }

    private fun powerLabel(src: Int): String = when (src) {
        1 -> "HT-36V"
        2 -> "BT service"
        3 -> "USB-C"
        else -> "Inconnu"
    }

    private fun present(v: Boolean) = if (v) "PRÉSENT" else "ABSENT"
    private fun ok(v: Boolean) = if (v) "OK" else "ABSENT"
}

object VentecBleParser {
    private fun buf(bytes: ByteArray): ByteBuffer =
        ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN)

    fun parseCurrent(bytes: ByteArray): Float? =
        if (bytes.size < 4) null else buf(bytes).float

    fun parseTemps(bytes: ByteArray): VentecTemps? {
        if (bytes.size < 12) return null
        val b = buf(bytes)
        return VentecTemps(b.float, b.float, b.float)
    }

    fun parseAlarmLevel(bytes: ByteArray): Int? =
        bytes.firstOrNull()?.toInt()

    fun parseStatus(bytes: ByteArray): VentecStatus? {
        if (bytes.size < 4) return null
        return VentecStatus(
            powerSource = bytes[0].toInt() and 0xFF,
            vibrationPresent = bytes[1] != 0.toByte(),
            humidityPresent = bytes[2] != 0.toByte(),
            loraPresent = bytes[3] != 0.toByte(),
        )
    }

    fun parseTelemetry(bytes: ByteArray): VentecTelemetry? {
        if (bytes.size < 10) return null
        val b = buf(bytes)
        val humidity = b.float
        val vibration = b.float
        val sensors = b.get().toInt() and 0xFF
        val modes = b.get().toInt() and 0xFF
        return VentecTelemetry(
            humidityPct = humidity,
            vibrationMg = vibration,
            inaOk = sensors and 0x01 != 0,
            tmpOk = sensors and 0x02 != 0,
            vibrationModule = sensors and 0x04 != 0,
            humidityModule = sensors and 0x08 != 0,
            loraModule = sensors and 0x10 != 0,
            labActive = modes and 0x01 != 0,
        )
    }

    fun parseAlarmEvent(bytes: ByteArray): VentecAlarmEvent? {
        if (bytes.size < 14) return null
        val b = buf(bytes)
        return VentecAlarmEvent(
            timestampMs = b.int.toLong() and 0xFFFFFFFFL,
            type = b.get().toInt() and 0xFF,
            level = b.get().toInt() and 0xFF,
            triggerValue = b.float,
            threshold = b.float,
        )
    }
}
