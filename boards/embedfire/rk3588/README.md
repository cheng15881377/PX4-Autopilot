# EmbedFire LubanCat RK3588 (Prototype)

This board target is an initial AArch64 Linux port for bringing up PX4 on an
EmbedFire LubanCat RK3588. It is not flight-tested.

## Prototype hardware mapping

The initial mapping is deliberately small and must be matched to the target
kernel's device tree before real hardware is used:

| Function | Prototype mapping |
| --- | --- |
| IMU | ICM42688P on SPI0 CS0 (`/dev/spidev0.0`) |
| Motor output | PCA9685 on `/dev/i2c-4`, address `0x40` |
| RC input | UART1 on `/dev/ttyS1` (receiver protocol dependent) |

The LubanCat-5 V2 boot configuration must enable the `spi0-m1` GPIO chip-select,
`i2c4-m3`, and `uart1-m1` overlays. Update `src/spi.cpp`, `src/i2c.cpp`,
`init/rc.board_sensors`, and `init/rc.board_extras` if the target kernel exposes
different device numbers.

## Cross-compile

Install an AArch64 Linux cross-compiler in the Ubuntu VM and build:

```sh
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
make embedfire_rk3588_default
```

The target root filesystem must provide a glibc version compatible with the
cross-compiler sysroot. Prefer matching the Ubuntu release in the VM to the
release running on the LubanCat.

## Upload

Set the LubanCat SSH host and user, then upload the binary, generated `etc`
directory, and startup file:

```sh
export AUTOPILOT_HOST=192.168.1.100
export AUTOPILOT_USER=cat
make embedfire_rk3588_default upload
```

## Run on CPU 7

The upload includes helper scripts that start PX4 with every inherited thread
affined to logical CPU 7 and completely stop both running and suspended PX4
processes:

```sh
cd /home/cat/px4
./run_px4.sh
./stop_px4.sh
```

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
