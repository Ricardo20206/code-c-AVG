package com.ventec.ble

import java.util.UUID

object VentecBleUuids {
    const val DEVICE_NAME = "Ventec-AGV-Monitor"

    val SERVICE: UUID = uuid("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
    val CHAR_CURRENT: UUID = uuid("6e400002-b5a3-f393-e0a9-e50e24dcca9e")
    val CHAR_TEMPS: UUID = uuid("6e400003-b5a3-f393-e0a9-e50e24dcca9e")
    val CHAR_ALARM: UUID = uuid("6e400004-b5a3-f393-e0a9-e50e24dcca9e")
    val CHAR_THRESH: UUID = uuid("6e400005-b5a3-f393-e0a9-e50e24dcca9e")
    val CHAR_LOGS: UUID = uuid("6e400006-b5a3-f393-e0a9-e50e24dcca9e")
    val CHAR_STATUS: UUID = uuid("6e400007-b5a3-f393-e0a9-e50e24dcca9e")
    val CHAR_TELEMETRY: UUID = uuid("6e400008-b5a3-f393-e0a9-e50e24dcca9e")
    val CHAR_ALARM_EVENT: UUID = uuid("6e400009-b5a3-f393-e0a9-e50e24dcca9e")
    val CHAR_ALARM_HIST: UUID = uuid("6e40000a-b5a3-f393-e0a9-e50e24dcca9e")

    val NOTIFY_CHARACTERISTICS = listOf(
        CHAR_CURRENT,
        CHAR_TEMPS,
        CHAR_ALARM,
        CHAR_STATUS,
        CHAR_TELEMETRY,
        CHAR_ALARM_EVENT,
    )

    private fun uuid(s: String): UUID = UUID.fromString(s)
}
