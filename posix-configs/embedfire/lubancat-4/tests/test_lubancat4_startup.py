#!/usr/bin/env python3
"""Exercise the LubanCat-4 shell startup with mocked PX4 modules, no hardware.

Run: python3 -m unittest discover -s posix-configs/embedfire/lubancat-4/tests -v
"""

import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


REPO = Path(__file__).resolve().parents[4]
BOARD = REPO / "boards/embedfire/lubancat-4"
STARTUP = REPO / "posix-configs/embedfire/lubancat-4/lubancat4_mc.config"


class LubanCat4StartupTest(unittest.TestCase):
    def run_startup(self, script=STARTUP, missing=(), fail_module=""):
        # Use /dev/null symlinks as private character-device stand-ins. Only
        # shell predicates inspect them; mocked PX4 modules never open hardware.
        with tempfile.TemporaryDirectory(prefix="px4-lubancat4-test-") as tmp:
            work = Path(tmp)
            dev = work / "dev"
            dev.mkdir()
            for name in ("ttyS0", "ttyS6", "ttyS7", "ttyS9"):
                if name not in missing:
                    (dev / name).symlink_to("/dev/null")

            init = work / "etc/init.d"
            init.mkdir(parents=True)
            for source in (BOARD / "init").iterdir():
                (init / source.name).write_text(
                    source.read_text().replace("/dev/", str(dev) + "/")
                )

            modules = (
                "param dataman load_mon battery_status icm42688p ms5611 "
                "qmc5883l gps pca9685_pwm_out crsf_rc rc_update manual_control "
                "sensors commander navigator ekf2 land_detector "
                "mc_hover_thrust_estimator flight_mode_manager mc_pos_control "
                "mc_att_control mc_rate_control control_allocator mavlink logger"
            ).split()
            aliases = [
                'mock_px4() { printf "%s\\n" "$*" >> "$COMMAND_LOG"; '
                '[ "$1" != "$FAIL_MODULE" ] || [ "$2" != start ]; }'
            ]
            aliases.extend(
                name + '() { mock_px4 ' + name + ' "$@"; }'
                for name in modules
            )
            (work / "px4-alias.sh").write_text("\n".join(aliases) + "\n")
            entry = work / "startup.sh"
            entry.write_text(
                ". px4-alias.sh\n"
                + script.read_text().replace("/dev/", str(dev) + "/")
            )
            log = work / "commands.log"
            env = dict(os.environ, PATH=str(work) + os.pathsep + os.defpath,
                       COMMAND_LOG=str(log), FAIL_MODULE=fail_module)
            result = subprocess.run(
                ["sh", str(entry)], cwd=work, env=env,
                text=True, capture_output=True, check=False
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stderr, "")
            commands = log.read_text().replace(str(dev) + "/", "/dev/")
            output = result.stdout.replace(str(dev) + "/", "/dev/")
            return commands.splitlines(), output

    def test_sensor_bus_and_uart_mapping(self):
        commands, output = self.run_startup(BOARD / "init/rc.board_sensors")
        self.assertEqual(commands, [
            "icm42688p start -s -b 0 -c 0 -R 0",
            "ms5611 start -X -b 6 -a 0x77",
            "qmc5883l start -X -b 5 -a 0x0d -f 50 -R 8",
            "gps start -d /dev/ttyS0 -b 115200 -p ubx",
            "pca9685_pwm_out start -b 8 -a 0x40",
        ])
        self.assertEqual(output, "")

    def test_registered_i2c_buses(self):
        buses = re.findall(r"initI2CBusExternal\((\d+)\)",
                           (BOARD / "src/i2c.cpp").read_text())
        self.assertEqual(buses, ["5", "6", "8"])

    def test_board_and_deployment_are_independent(self):
        identity = re.search(r'BOARD_OVERRIDE_UUID "([^"]+)"',
                             (BOARD / "src/board_config.h").read_text()).group(1)
        self.assertEqual(len(identity), 16)
        legacy = REPO / "boards/embedfire/rk3588"
        self.assertNotIn(identity, (legacy / "src/board_config.h").read_text())
        legacy_buses = re.findall(r"initI2CBusExternal\((\d+)\)",
                                 (legacy / "src/i2c.cpp").read_text())
        self.assertEqual(legacy_buses, ["2", "4", "8"])
        self.assertIn("TELEMETRY_DEVICE=/dev/ttyUSB0",
                      (REPO / "posix-configs/embedfire/rk3588_mc.config").read_text())
        upload = (BOARD / "cmake/upload.cmake").read_text()
        self.assertIn("posix-configs/embedfire/lubancat-4/lubancat4_mc.config", upload)
        self.assertIn("/px4-lubancat4", upload)
        self.assertIn("DEPENDS px4 romfs_gen_files_target", upload)
        runtime = STARTUP.parent
        self.assertIn('PX4_CONFIG="${SCRIPT_DIR}/lubancat4_mc.config"',
                      (runtime / "run_px4.sh").read_text())
        self.assertIn('SERVICE_NAME="px4-lubancat4.service"',
                      (runtime / "install_px4_autostart.sh").read_text())

    def test_full_startup_has_separate_native_uart_links(self):
        commands, output = self.run_startup()
        self.assertIn("crsf_rc start -d /dev/ttyS6", commands)
        self.assertEqual([c for c in commands if c.startswith("mavlink ")], [
            "mavlink start -x -u 14556 -r 1000000 -p",
            "mavlink start -Z -d /dev/ttyS9 -b 57600 -r 1200",
            "mavlink start -Z -d /dev/ttyS7 -b 115200 -r 1000 -m custom",
            "mavlink boot_complete",
        ])
        self.assertEqual(output, "")
        self.assertFalse(any("ttyUSB" in c for c in commands))
        self.assertEqual(commands.count("rc_update start"), 1)

    def test_installer_does_not_stop_a_legacy_service(self):
        for state in ("active", "enabled"):
            with self.subTest(state=state), tempfile.TemporaryDirectory() as tmp:
                work = Path(tmp)
                log = work / "systemctl.log"
                sudo = work / "sudo"
                sudo.write_text(
                    '#!/bin/sh\n[ "$1" = -v ] && exit 0\n'
                    '[ "$1" = -n ] && shift\nexec "$@"\n'
                )
                systemctl = work / "systemctl"
                systemctl.write_text(
                    '#!/bin/sh\nprintf "%s\\n" "$*" >> "$COMMAND_LOG"\n'
                    '[ "$3" = px4-rk3588.service ] || exit 99\n'
                    'case "$1" in\n'
                    'is-active) [ "$LEGACY_STATE" = active ];;\n'
                    'is-enabled) [ "$LEGACY_STATE" = enabled ];;\n'
                    '*) exit 99;;\nesac\n'
                )
                sudo.chmod(0o755)
                systemctl.chmod(0o755)
                env = dict(os.environ, PATH=str(work) + os.pathsep + os.defpath,
                           COMMAND_LOG=str(log), LEGACY_STATE=state)
                result = subprocess.run(
                    ["bash", str(STARTUP.parent / "install_px4_autostart.sh"), "install"],
                    env=env, text=True, capture_output=True, check=False
                )
                self.assertEqual(result.returncode, 1)
                self.assertIn("px4-rk3588.service", result.stderr)
                self.assertTrue(all(line.startswith(("is-active ", "is-enabled "))
                                    for line in log.read_text().splitlines()))

    def test_defaults_preserve_saved_flight_configuration(self):
        commands, _ = self.run_startup(BOARD / "init/rc.board_defaults")
        self.assertIn("param set-default MAV_PROTO_VER 1", commands)
        self.assertIn("param set-default EKF2_OF_CTRL 1", commands)
        self.assertIn("param set-default EKF2_RNG_CTRL 1", commands)
        self.assertTrue(all(c.startswith("param set-default ") for c in commands))
        self.assertFalse(any("EKF2_HGT_REF" in c for c in commands))
        self.assertFalse(any("EKF2_GPS_CTRL" in c for c in commands))

    def test_absent_uarts_are_reported_without_usb_fallback(self):
        commands, output = self.run_startup(
            missing=("ttyS0", "ttyS6", "ttyS7", "ttyS9")
        )
        self.assertFalse(any(c.startswith(("gps ", "crsf_rc ")) for c in commands))
        self.assertFalse(any("mavlink start -Z" in c for c in commands))
        for port in ("ttyS0", "ttyS6", "ttyS7", "ttyS9"):
            self.assertIn("/dev/" + port + " not found", output)
        self.assertIn("logger start -t -b 200", commands)

    def test_missing_radio_does_not_disable_flow(self):
        commands, output = self.run_startup(missing=("ttyS9",))
        self.assertIn("/dev/ttyS9 not found", output)
        self.assertIn(
            "mavlink start -Z -d /dev/ttyS7 -b 115200 -r 1000 -m custom",
            commands
        )

    def test_start_failures_are_reported(self):
        for module, diagnostic in (
            ("crsf_rc", "BAYCKRC CRSF failed to start on /dev/ttyS6"),
            ("gps", "MG-A01 GNSS failed to start on /dev/ttyS0"),
            ("mavlink", "MTF-01P flow/range failed to start on /dev/ttyS7"),
            ("ms5611", "MS5611 not found on /dev/i2c-6 address 0x77"),
        ):
            with self.subTest(module=module):
                _, output = self.run_startup(fail_module=module)
                self.assertIn(diagnostic, output)


if __name__ == "__main__":
    unittest.main()
