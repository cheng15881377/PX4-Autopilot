# EmbedFire LubanCat-4 RK3588S (Prototype)

This board target is an initial AArch64 Linux port for bringing up PX4 on an
original EmbedFire LubanCat-4 (RK3588S), running Ubuntu 20.04. It is not
flight-tested. This mapping is not for the LubanCat-4-V1 or LubanCat-5-V2.
This is an independent target, `embedfire_lubancat-4_default`. The original
`boards/embedfire/rk3588` target and its startup scripts retain their original
LubanCat-5-V2 mapping. Never deploy one board's configuration to the other.

The independent runtime files live in `posix-configs/embedfire/lubancat-4/`.
They use `lubancat4_mc.config`, upload to `/home/<user>/px4-lubancat4`, and
install `px4-lubancat4.service`. The old target keeps `/home/<user>/px4`,
`rk3588_mc.config`, and `px4-rk3588.service`. Each runtime directory has its
own `parameters.bson`; no old parameters or calibrations are copied implicitly.
Only one PX4 process should run on a physical board at a time.

This Linux target uses the vendor U-Boot/Linux boot chain, not a PX4 NuttX
bootloader. It does not allocate a NuttX firmware board ID or a USB VID/PID.
Its 16-character local prototype UUID is distinct from the old target; an
official architecture identity and flight validation are still required before
upstream support can be claimed. The repository's build-all CI automatically
discovers the new `.px4board` target in its AArch64 Linux group.

## Prototype hardware mapping

All header numbers below are physical 40-pin header positions, not GPIO
numbers. Board TX connects to module RX, and board RX to module TX. Find
pin 1 from the board silkscreen before connecting power.

| Function | Linux interface | Physical pins (board perspective) |
| --- | --- | --- |
| ICM42688P IMU | SPI0-M2 CS0, `/dev/spidev0.0` | 19 MOSI, 21 MISO, 23 CLK, 24 CS0 |
| MS5611 barometer | I2C6-M3, `/dev/i2c-6`, `0x77` | 27 SDA, 28 SCL |
| MG-A01 QMC5883L | I2C5-M3, `/dev/i2c-5`, `0x0d` | 3 SDA, 5 SCL |
| MG-A01 GNSS | UART0-M2, `/dev/ttyS0`, UBX 115200 | 8 TX, 10 RX |
| BAYCKRC ELRS receiver | UART6-M1, `/dev/ttyS6`, CRSF | 11 TX, 7 RX |
| MTF-01P flow/range | UART7-M1, `/dev/ttyS7`, MAVLink 115200 | 37 TX, 16 RX (not 15) |
| LR900 telemetry | UART9-M2, `/dev/ttyS9`, MAVLink 57600 | 35 TX, 22 RX |
| PCA9685 motor output | I2C8-M2, `/dev/i2c-8`, `0x40` | 32 SDA, 12 SCL |

Unallocated signal pins are 13, 15, 18, 26, 29, 31, 33, 36, 38 and 40.
Some can still be reserved by Linux pinctrl (for example SPI CS1 or UART
RTS/CTS), so this is not a guarantee that all ten are available as GPIO.

### Power And Module Pins

- Pins 1 and 17 provide 3.3 V; pins 2 and 4 provide 5 V.
- Ground pins are 6, 9, 14, 20, 25, 30, 34 and 39 (0 V).
- All SPI, I2C and UART signals must be compatible with 3.3 V. I2C pull-ups
  must not pull the board pins up to 5 V. Do not use RS-232 or RS-485 directly.
- MG-A01 uses one 5 V supply for both GNSS and magnetometer. MTF-01P and the
  pictured BAYCKRC receiver also use 5 V power, not 3.3 V power.
- LR900 uses 4.5-5 V power and 3.3 V UART signalling. Use a suitable regulated
  5 V supply and common ground; its A/F/P variants have different power needs.
  Do not parallel an external supply's positive lead with header pins 2/4.
- The pictured ICM42688P module has separate `VCC` and `3V3` pads. A 3.3 V
  direct supply to `3V3`, leaving `VCC` open, is only a proposed connection
  until the seller confirms that this revision supports it. Never apply
  5 V to `3V3`. For SPI, `SDA/MOSI` goes to 19, `AD0/MISO` to 21,
  `SCL/SCLK` to 23 and `CS` to 24. Leave `INT1`, `INT2` and `NC` open;
  this target uses polling rather than a wired data-ready interrupt.
- The pictured MS5611 has `VCC, GND, SCL, SDA, CSB, SDO, PS` pads. Confirm
  the breakout's supply circuit before using the proposed 3.3 V `VCC` feed.
  `PS` must be high at the sensor's VDD level for I2C; `CSB` must be grounded
  for address `0x77` (high selects `0x76`). Leave `SDO` open. Check existing
  solder bridges before tying a mode pin to the opposite rail.
- PCA9685 logic `VCC` is 3.3 V. Its load supply `V+` stays disconnected when
  only sending PWM to separately powered ESCs. Check that `OE` is held low,
  not floating, and address jumpers select `0x40`. ESC signal grounds must
  be common, but motor current must not flow through header ground wires.
  ESC BEC positive wires are not paralleled into this supply arrangement.

These supply checks cannot be performed by PX4. Use the original board's
specified Type-C power input; do not assume the 4-V1's header power-input
features apply. Disconnect power while rewiring and attach RF antennas before
powering the receiver/radio.

### Linux Device Tree

PX4 does not configure Rockchip pin multiplexing. First check the actual
board model and the active boot configuration:

```sh
tr -d '\0' </proc/device-tree/model
uname -r
ls -l /boot/uEnv/uEnv.txt
```

For the original board the vendor configuration is `uEnvLubanCat4.txt`, not
`uEnvLubanCat4-V1.txt`. Back it up before editing. Enable the vendor overlays
for `spi0-m2`, `i2c8-m2`, `uart0-m2`, `uart6-m1`, `uart7-m1` and `uart9-m2`.
I2C5-M3 and I2C6-M3 must also be enabled; these are normally in the original
board's base device tree. Exact overlay filenames and availability depend on
the installed kernel image. Confirm the SPI overlay exposes CS0 as spidev.

Disable conflicting alternatives on these pins, including `spi0-m1`,
`uart4-m2`, `uart7-m2`, I2C2-M4 and I2C3-M1, and overlapping PWM functions.
Do not enable two mux routes for the same controller. Release any serial
console or `serial-getty` service on UART0/6/7/9. No RTS/CTS wires are used.
After reboot, require these device nodes before running PX4:

```sh
ls -l /dev/spidev0.0 /dev/i2c-5 /dev/i2c-6 /dev/i2c-8
ls -l /dev/ttyS0 /dev/ttyS6 /dev/ttyS7 /dev/ttyS9
```

Device-node existence alone does not prove the correct pin mux; verify the
kernel pinctrl configuration and wiring as well. The launch script runs PX4
with root privileges, so a driver-open failure is not fixed by changing UART
permissions blindly.

### MTF-01P And Existing Parameters

Set the module to `Mavlink_px4` (or `AUTO`) at 115200 baud. The dedicated
MAVLink instance on `/dev/ttyS7` uses custom mode: it sends the standard 1 Hz
PX4 heartbeat for detection, receives `OPTICAL_FLOW_RAD` and `DISTANCE_SENSOR`,
and does not stream high-rate flight data back to the sensor. Forwarding and
hardware flow control are off. No additional optical-flow driver is needed.
LR900 uses its own instance on `/dev/ttyS9`; LAN discovery remains on UDP.

Board defaults use `MAV_PROTO_VER=1` for the MTF-01P's PX4 auto-detection,
`EKF2_OF_CTRL=1`, and conditional range aiding (`EKF2_RNG_CTRL=1`). They do not
change the height reference, GNSS fusion, or saved calibration. Imported
parameters take precedence over `param set-default`. On an existing install,
check these in the PX4 console and deliberately set the protocol if necessary:

```sh
param show MAV_PROTO_VER
param set MAV_PROTO_VER 1
param save
```

Restart PX4 after this change. Do not reset all parameters: preserve actuator
mapping, tuning and calibration. Do not set `MAV_*_CONFIG` serial-port
parameters for these links; this POSIX startup launches the instances directly.

IMU rotation remains `-R 0`; the previous MG-A01 rotation `-R 8`
(`ROTATION_ROLL_180`, not yaw 180) is retained. These are mounting assumptions,
not consequences of the pin mapping. Verify them and `SENS_FLOW_ROT` after
installation. The magnetometer's bus change also changes its device ID;
recalibrate/reselect the compass on the new bus before flight.

With props removed, check in the PX4 console:

```sh
icm42688p status
ms5611 status
qmc5883l status
gps status
crsf_rc status
pca9685_pwm_out status
mavlink status
listener sensor_optical_flow -n 5
listener vehicle_optical_flow -n 5
listener distance_sensor -n 5
```

Check message rates, range direction, quality, scaling, mounting rotations,
RC failsafe and disabled outputs before using flow/range for control. A
successful build or visible distance messages do not establish safe hovering.

Reference: [EmbedFire 40-pin mapping](https://doc.embedfire.com/linux/rk3588/quick_start/zh/latest/quick_start/40pin/40pin.html#lubancat-4),
[boot configuration](https://doc.embedfire.com/linux/rk3588/quick_start/zh/latest/quick_start/device_tree/device_tree.html),
[MTF-01P manual](https://micoair.cn/zh/docs/sensors/sensors/mtf-01p-sensors).

## Cross-compile

Install an AArch64 Linux cross-compiler in the Ubuntu VM and build:

```sh
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
make embedfire_lubancat-4_default
```

The target root filesystem must provide a glibc version compatible with the
cross-compiler sysroot. Prefer matching the Ubuntu release in the VM to the
release running on the LubanCat.

The result is `build/embedfire_lubancat-4_default/bin/px4` plus its generated `etc`
directory, not a NuttX `.px4` image. For Ubuntu 20.04, use a compatible AArch64
glibc sysroot (2.31); a binary linked against a newer glibc may not run there.

The hardware-free shell startup regression tests can run on the host:

```sh
python3 -m unittest discover -s posix-configs/embedfire/lubancat-4/tests -v
```

## Upload

Set the LubanCat SSH host and user, then upload the binary, generated `etc`
directory, and startup file:

```sh
export AUTOPILOT_HOST=192.168.1.100
export AUTOPILOT_USER=cat
make embedfire_lubancat-4_default upload
```

## Run on CPU 7

The upload includes helper scripts that start PX4 with every inherited thread
affined to logical CPU 7 and completely stop both running and suspended PX4
processes:

```sh
cd /home/cat/px4-lubancat4
./run_px4.sh
./stop_px4.sh
```

The new upload also includes `install_px4_autostart.sh`. Before enabling its
service, explicitly disable any old `px4-rk3588.service` on that board. The
installer refuses to install/start the new service while the old one is active
or enabled, rather than stopping or deleting the old service automatically.
Both launchers reject an already running PX4 process; their lock/socket
instance is shared, so the two versions must not run concurrently.

For lower scheduling jitter, isolate CPU 7 in the target kernel command line
and keep ordinary IRQs on CPUs 0-6:

```text
isolcpus=domain,managed_irq,7 nohz_full=7 rcu_nocbs=7 irqaffinity=0-6
```

Verify the boot configuration and PX4 thread placement:

```sh
cat /proc/cmdline
cat /sys/devices/system/cpu/isolated
pid=$(pidof px4)
taskset -pc "$pid"
ps -T -p "$pid" -o pid,tid,psr,cls,rtprio,pri,comm
```

Do not install propellers during bring-up. Validate IMU data, RC failsafe, motor
ordering, output disarming, scheduling latency, and power monitoring before any
flight test.
