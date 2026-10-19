# Embedded Workshop

Firmware for an STM32F407 Discovery board that joins a wired network and reports its readings to remote services.

## Language

**Device ID**:
The STM32's factory-programmed 96-bit unique identifier, written as 24 uppercase hex characters, highest-address word first. It names this board on the network.
_Avoid_: CPUID, chip ID, serial number

**Temperature topic**:
The MQTT topic `SBC/<Device ID>/temperature` on which the board publishes its temperature.

**Humidity topic**:
The MQTT topic `SBC/<Device ID>/humidity` on which the board publishes its relative humidity, in whole percent.

**Acceleration topic**:
The MQTT topic `SBC/<Device ID>/acceleration` on which the board publishes its three-axis acceleration, in g.

**Angular rate topic**:
The MQTT topic `SBC/<Device ID>/angular_rate` on which the board publishes its three-axis angular rate, in degrees per second.

**Magnetic field topic**:
The MQTT topic `SBC/<Device ID>/magnetic_field` on which the board publishes its three-axis magnetic field, in gauss.

**Climate sensor**:
The DHT11 module on the board, the single source of both the board's temperature and its relative humidity.
_Avoid_: DHT, humidity sensor, temperature sensor

**Motion sensor**:
The LSM9DS1 module on the board, the single source of the board's acceleration, angular rate and magnetic field. All three readings share one set of axes: those of its accelerometer and gyroscope. Its die temperature is not used.
_Avoid_: MEMS, IMU, LSM, accelerometer (for the whole module)

**Reporting threshold**:
The smallest change in a reading, in that reading's own units, that a consumer of the Climate sensor is told about. A reading is reported when it differs from the last value reported to that consumer by at least the threshold. For a three-axis reading, it is reported when any one axis differs by at least the threshold. Each consumer chooses its own when it subscribes.
_Avoid_: delta, hysteresis, dead-band
