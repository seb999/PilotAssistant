#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "icm20948_sensor.h"

int main(void) {
    stdio_init_all();
    while (!stdio_usb_connected()) sleep_ms(100);
    sleep_ms(500);

    printf("=== ICM20948 SPI Test ===\n");
    printf("Pins: NCS=GPIO%d  SCK=GPIO%d  MOSI=GPIO%d  MISO=GPIO%d\n",
           ICM20948_CS_PIN, ICM20948_SCK_PIN, ICM20948_MOSI_PIN, ICM20948_MISO_PIN);

    if (!icm20948_init()) {
        printf("INIT FAILED - check wiring\n");
        while (true) sleep_ms(1000);
    }

    SensorData accel, gyro;

    while (true) {
        if (icm20948_read_accel(&accel) && icm20948_read_gyro(&gyro)) {
            float ax = icm20948_accel_to_g(accel.x, ACCEL_RANGE_4G);
            float ay = icm20948_accel_to_g(accel.y, ACCEL_RANGE_4G);
            float az = icm20948_accel_to_g(accel.z, ACCEL_RANGE_4G);
            float gx = icm20948_gyro_to_dps(gyro.x, GYRO_RANGE_500DPS);
            float gy = icm20948_gyro_to_dps(gyro.y, GYRO_RANGE_500DPS);
            float gz = icm20948_gyro_to_dps(gyro.z, GYRO_RANGE_500DPS);

            printf("ACCEL  X=%+6.3fg  Y=%+6.3fg  Z=%+6.3fg  |  "
                   "GYRO  X=%+7.2f  Y=%+7.2f  Z=%+7.2f dps\n",
                   ax, ay, az, gx, gy, gz);
        } else {
            printf("read error\n");
        }
        sleep_ms(100);
    }
}
