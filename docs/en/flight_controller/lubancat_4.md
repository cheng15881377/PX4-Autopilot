# EmbedFire LubanCat-4 (Experimental)

<LinkedBadge type="warning" text="Experimental" url="../flight_controller/autopilot_experimental.md"/>

::: warning
This is a local prototype port, not a flight-validated or manufacturer-supported PX4 autopilot.
PX4 does not manufacture this hardware.
Contact [EmbedFire](https://www.embedfire.com/) for board hardware support; that does not imply manufacturer support for this PX4 integration.
No flight logs are available for this port.
Do not fit propellers during bring-up.
:::

This target is for the original LubanCat-4 with RK3588S, not LubanCat-4-V1 or LubanCat-5-V2.
It runs PX4 as an AArch64 Linux process on the user's Ubuntu 20.04 system.
The vendor U-Boot/Linux boot chain is used; there is no PX4 NuttX bootloader or flashable `.px4` image.
The existing `embedfire_rk3588_default` prototype remains separate and retains its former wiring.

## Hardware And Pinout

The connections use the board's 2×20, 2.54 mm expansion header.
All numbers below are physical header positions, not GPIO identifiers.
Board TX connects to module RX and board RX connects to module TX.

| Peripheral | Interface | Physical Header Pins |
| --- | --- | --- |
| ICM42688P | SPI0-M2 CS0, `/dev/spidev0.0` | MOSI 19, MISO 21, CLK 23, CS0 24 |
| MS5611 | I2C6-M3, `/dev/i2c-6`, `0x77` | SDA 27, SCL 28 |
| MG-A01 compass | I2C5-M3, `/dev/i2c-5`, `0x0d` | SDA 3, SCL 5 |
| MG-A01 GNSS | UART0-M2, `/dev/ttyS0`, 115200 | TX 8, RX 10 |
| BAYCKRC ELRS | UART6-M1, `/dev/ttyS6`, CRSF | TX 11, RX 7 |
| MTF-01P | UART7-M1, `/dev/ttyS7`, 115200 | TX 37, RX 16 (not 15) |
| LR900 | UART9-M2, `/dev/ttyS9`, 57600 | TX 35, RX 22 |
| PCA9685 | I2C8-M2, `/dev/i2c-8`, `0x40` | SDA 32, SCL 12 |

Power pins are 1/17 for 3.3 V, 2/4 for 5 V, and 6/9/14/20/25/30/34/39 for GND (0 V).
Signals must be compatible with 3.3 V, including I2C pull-ups.
Do not infer a breakout board's allowed power input from the sensor chip's limits or a `VCC` label.
In particular, the pictured ICM42688P has separate `VCC` and `3V3` pads; its supply circuit still requires confirmation.
PCA9685 logic `VCC` is 3.3 V; its load supply `V+` is not the logic supply.

Refer to the [EmbedFire pinout][pinout] and [boot configuration][boot] for the original board revision.
Board availability and purchase information are provided by [EmbedFire][vendor].
MTF-01P uses its [vendor MAVLink protocol][flow] on a GH1.25-4P connection; follow the module's signal labels rather than wire colours.

[pinout]: https://doc.embedfire.com/linux/rk3588/quick_start/zh/latest/quick_start/40pin/40pin.html#lubancat-4
[boot]: https://doc.embedfire.com/linux/rk3588/quick_start/zh/latest/quick_start/device_tree/device_tree.html
[vendor]: https://www.embedfire.com/
[flow]: https://micoair.cn/zh/docs/sensors/sensors/mtf-01p-sensors

## Build And Deploy

Build with an AArch64 Linux toolchain and a glibc sysroot compatible with the target OS:

```sh
make embedfire_lubancat-4_default
```

The executable is `build/embedfire_lubancat-4_default/bin/px4`.
The board sources live in `boards/embedfire/lubancat-4/`; runtime scripts live in `posix-configs/embedfire/lubancat-4/`.
The upload target installs into `/home/<user>/px4-lubancat4`, independently of the old target's `/home/<user>/px4` directory:

```sh
export AUTOPILOT_HOST=192.168.1.100
export AUTOPILOT_USER=cat
make embedfire_lubancat-4_default upload
```

Use the new directory's `run_px4.sh` and `stop_px4.sh` helpers.
The startup file is `lubancat4_mc.config`; the optional service is `px4-lubancat4.service`.
Each deployment directory keeps its own `parameters.bson`; calibration and actuator settings are not copied automatically.
Do not run the old and new versions concurrently on one board.

Enable the matching Linux device-tree interfaces before starting PX4.
Configure MTF-01P for `Mavlink_px4` or `AUTO`, and check `MAV_PROTO_VER=1` if using existing saved parameters.
Recalibrate the compass after its bus/device-ID change and verify all mounting rotations.
The port has no wired ADC power monitor; validate power monitoring, RC failsafe, actuator disarming and motor order before any flight attempt.

The board README contains the complete module supply caveats, device-node checks and bench-test commands.
