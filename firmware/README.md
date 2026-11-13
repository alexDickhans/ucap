# uCap v1 firmware

PlatformIO + Arduino (STM32 core) firmware for the STM32G474CEUx on uCap v1.

Pin map and nets come from `ucap/ucap.kicad_sch`, the BOM, and the board README — not invented.

## Build / flash

```bash
cd firmware
pio run
pio run -t upload   # ST-Link on J2 (SWDIO/SWCLK/GND only — no NRST on the connector)
pio device monitor
```

USB CDC enumerates as a serial port for telemetry.

## Capacitance (COMP1 + TIM2)

Schematic sheet **RC Delay Sensing**:

| Net | Pin | Role |
| --- | --- | --- |
| `current_insert` | PA0 | GPIO charge/discharge through R5 (1 MΩ) into H1 |
| `sense` | PA1 | `COMP1_INP` (also `ADC1_IN2`) |
| `v_th` | PA4 | `COMP1_INM` from R7/R8 = Vdd/2 (also `ADC2_IN17`) |

Measurement: discharge → arm COMP1 rising EXTI → start TIM2 → drive PA0 high → latch TIM2 on threshold.  
With Vth = 0.5·Vdd: `C = t / (R · ln(2))`, R = 1 MΩ.

## I2C buses (Qwiic J3–J6)

| Firmware bus | Silk | Conn | SDA | SCL | STM32 I2C | Default device |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | I2C 0 | J3 | PB9 | PA15 | I2C1 | Adafruit #4022 LIS3MDL |
| 1 | I2C 1 | J4 | PF0 | PC4 | I2C2 | VL53L5CX #0 |
| 2 | I2C 2 | J5 | PC11 | PA8 | I2C3 | VL53L5CX #1 |
| 3 | I2C 3 | J6 | PB7 | PC6 | I2C4 | VL53L5CX #2 |

Socket-to-sensor mapping is firmware policy (PCB sockets are identical). Change `BUS_*` in `include/board_pins.h` if you plug differently.

## External sensors

- **Magnetometer**: Adafruit #4022 = ST LIS3MDL on I2C0.
- **ToF**: SparkFun Qwiic Mini ToF Imager **VL53L5CX** (up to 3), each on its own bus (I2C1–I2C3). Uses the SparkFun Arduino library (ST ULD under the hood).

## Other MCU features wired on the board

- **STAT LED** D1 on PA5.
- **USB FS** PA11/PA12 — CDC console.
- **ADC** diagnostic reads of `sense` / `v_th` (same pads as COMP).
- **SWD** PA13/PA14 on J2 (no reset line on that connector).

## Power note (from schematic)

Peak ranging budget in the schematic text assumes 3× ToF imagers + mag; the board uses a buck (TPS563208), not an LDO, for that load.
