#pragma once

// PSRAM carte Ventec — ISSI IS66WVS1M8BLL-104NLI (1 Mo, quad SPI, 3.3 V)
// Câblage : ESP32-PICO-D4 ↔ PSRAM (schéma Ventec, conforme Table 3 Espressif)
//
// | Signal PSRAM | Pin U102 | Signal ESP32-PICO-D4 | GPIO |
// |--------------|----------|----------------------|------|
// | CE#          | 1        | SD_DATA_3            | 10   |
// | SO / SIO1    | 2        | GPIO17               | 17   |
// | SIO2         | 3        | SD_DATA_0            | 7    |
// | CLK          | 6        | SD_CLK               | 6    |
// | SI / SIO0    | 5        | SD_DATA_1            | 8    |
// | SIO3         | 7        | SD_CMD               | 11   |
//
// Config firmware : sdkconfig.defaults (CLK=6, CS=10)

#define VENTEC_PSRAM_CHIP_NAME      "IS66WVS1M8BLL-104NLI"
#define VENTEC_PSRAM_NOMINAL_BYTES  (1024U * 1024U)

#define VENTEC_PSRAM_GPIO_CLK       6
#define VENTEC_PSRAM_GPIO_CS        10

#define PSRAM_ALARM_MAX             50
#define PSRAM_MEAS_RING_SIZE        128
