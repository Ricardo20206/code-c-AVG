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

    fun parseThresholds(bytes: ByteArray): VentecThresholds? {
        if (bytes.size < 16) return null
        val b = buf(bytes)
        return VentecThresholds(b.float, b.float, b.float, b.float)
    }

    fun encodeThresholds(t: VentecThresholds): ByteArray {
        val b = ByteBuffer.allocate(16).order(ByteOrder.LITTLE_ENDIAN)
        b.putFloat(t.warnA)
        b.putFloat(t.alarmA)
        b.putFloat(t.tempC)
        b.putFloat(t.driftA)
        return b.array()
    }

    fun parseAlarmHistory(bytes: ByteArray): List<VentecAlarmEvent> {
        if (bytes.isEmpty()) return emptyList()
        val count = bytes[0].toInt() and 0xFF
        val out = ArrayList<VentecAlarmEvent>(count)
        var offset = 1
        repeat(count) {
            if (offset + 14 > bytes.size) return out
            out += parseAlarmEvent(bytes.copyOfRange(offset, offset + 14)) ?: return out
            offset += 14
        }
        return out
    }
}
