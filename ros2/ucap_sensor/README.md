# ucap_sensor

ROS 2 (`ament_python`) host for **uCap v1** USB CDC telemetry. Parses the text lines printed by `firmware/src/main.cpp` and publishes standard messages. Baud default **115200** matches `firmware/platformio.ini` (`monitor_speed`).

## Build

```bash
# From a ROS 2 workspace that contains this package (or symlink ros2/ucap_sensor into src/)
cd ~/ros2_ws/src
ln -s /path/to/ucap/ros2/ucap_sensor .
cd ~/ros2_ws
rosdep install --from-paths src -y --ignore-src
colcon build --packages-select ucap_sensor
source install/setup.bash
```

System dependency: `python3-serial` (pyserial).

## Run

```bash
ros2 launch ucap_sensor ucap.launch.py port:=/dev/ttyACM0 baud:=115200
# or
ros2 run ucap_sensor ucap_sensor_node --ros-args -p port:=/dev/ttyACM0 -p baud:=115200
```

Parameters: `port`, `baud` (default 115200), `frame_id`, `publish_raw`, `tof_*`, `reconnect_period_s`, `read_period_s`. See `config/ucap_sensor.yaml`.

## USB CDC serial format (from firmware)

Boot / status (not published as sensors; optional on `~/raw`):

```
uCap v1
pins: PA0 charge, PA1 COMP1_INP, PA4 COMP1_INM, PA5 LED
LIS3MDL ok on I2C0 (J3)          # or: LIS3MDL missing on I2C0
VL53L5CX present: <N>
```

Telemetry every ~200 ms (`Serial.println` fields, space-separated):

| Field | Firmware emission | Notes |
| --- | --- | --- |
| Capacitance | `C_fF=<float>` or `C_fF=timeout` | femtofarads; `t_us=` only when valid |
| Charge time | `t_us=<float>` | microseconds; omitted on timeout |
| Sense ADC | `sense_V=<float>` | volts |
| Threshold ADC | `vth_V=<float>` | volts |
| Magnetometer | `mag_uT=<x>,<y>,<z>` | microtesla; omitted if LIS3MDL invalid |
| ToF 0..2 | `tofN=-` \| `tofN=wait` \| `tofN=<int>mm` | center zone (index 5) of 4×4; `-` = absent |

Example:

```
C_fF=1234.5 t_us=0.86 sense_V=1.650 vth_V=1.648 mag_uT=12.3,-4.5,50.1 tof0=320mm tof1=- tof2=wait
```

STAT LED is toggled on each print in firmware but is **not** encoded on the wire.

## Topics (node namespaced under `ucap/` when launched)

| Topic | Type | Content |
| --- | --- | --- |
| `capacitance` | `std_msgs/Float64` | Farads (converted from `C_fF`) |
| `capacitance_fF` | `std_msgs/Float64` | Femtofarads as printed |
| `capacitance_valid` | `std_msgs/Bool` | False on `timeout` |
| `charge_time_us` | `std_msgs/Float64` | Microseconds |
| `adc/sense_voltage` | `std_msgs/Float64` | Volts |
| `adc/vth_voltage` | `std_msgs/Float64` | Volts |
| `magnetic_field` | `sensor_msgs/MagneticField` | Tesla (`uT × 1e-6`) |
| `tof0/range` .. `tof2/range` | `sensor_msgs/Range` | Meters (`mm × 1e-3`); skipped if `-` or `wait` |
| `tof0/present` .. `tof2/present` | `std_msgs/Bool` | Device present on that bus |
| `raw` | `std_msgs/String` | Full CDC line (disable with `publish_raw:=false`) |

## Tests (parser only)

```bash
cd ros2/ucap_sensor
PYTHONPATH=. python3 -m pytest test/test_serial_parser.py -q
```
