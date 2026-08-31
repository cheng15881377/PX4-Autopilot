#!/usr/bin/env bash

set -euo pipefail

# ======================== 常用配置：需要时改这里 ========================
SERVICE_NAME="px4-rk3588.service"
RESTART_DELAY_SECONDS="3"
# ======================================================================

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
RUN_SCRIPT="${SCRIPT_DIR}/run_px4.sh"
STOP_SCRIPT="${SCRIPT_DIR}/stop_px4.sh"
SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}"

usage()
{
	cat <<EOF
用法：
  $0 install    安装服务并立即启动（默认）
  $0 remove     停止并删除服务
  $0 start      启动 PX4 服务
  $0 stop       停止 PX4 服务
  $0 restart    重启 PX4 服务
  $0 status     查看服务状态
  $0 log        持续查看 PX4 日志
EOF
}

if (( EUID == 0 )); then
	SUDO=()
else
	sudo -v
	SUDO=(sudo -n)
fi

require_systemd()
{
	if ! command -v systemctl >/dev/null 2>&1; then
		echo "错误：当前系统没有 systemctl，无法安装开机启动服务。" >&2
		exit 1
	fi
}

install_service()
{
	local temporary_service

	if [[ ! -x "${RUN_SCRIPT}" || ! -x "${STOP_SCRIPT}" ]]; then
		echo "错误：run_px4.sh 或 stop_px4.sh 不存在或没有执行权限。" >&2
		exit 1
	fi

	temporary_service="$(mktemp)"
	trap 'rm -f -- "${temporary_service}"' RETURN

	cat >"${temporary_service}" <<EOF
[Unit]
Description=PX4 Autopilot on Embedfire LubanCat RK3588
Wants=network-online.target
After=network-online.target

[Service]
Type=simple
User=root
WorkingDirectory=${SCRIPT_DIR}
ExecStart=${RUN_SCRIPT} --daemon
ExecStop=${STOP_SCRIPT}
Restart=on-failure
RestartSec=${RESTART_DELAY_SECONDS}
TimeoutStopSec=15
KillMode=control-group

[Install]
WantedBy=multi-user.target
EOF

	"${SUDO[@]}" install -m 0644 "${temporary_service}" "${SERVICE_FILE}"
	"${SUDO[@]}" systemctl daemon-reload

	# A manually launched PX4 would conflict with the systemd-managed process.
	# Stop it cleanly before handing process ownership to systemd.
	"${SUDO[@]}" "${STOP_SCRIPT}"
	"${SUDO[@]}" systemctl enable "${SERVICE_NAME}"
	"${SUDO[@]}" systemctl restart "${SERVICE_NAME}"

	echo
	echo "PX4 开机自动启动已启用。"
	echo "查看状态：$0 status"
	echo "查看日志：$0 log"
}

remove_service()
{
	"${SUDO[@]}" systemctl disable --now "${SERVICE_NAME}" 2>/dev/null || true
	"${SUDO[@]}" rm -f -- "${SERVICE_FILE}"
	"${SUDO[@]}" systemctl daemon-reload
	"${SUDO[@]}" systemctl reset-failed "${SERVICE_NAME}" 2>/dev/null || true
	echo "PX4 开机自动启动服务已删除。"
}

require_systemd

case "${1:-install}" in
	install)
		install_service
		;;
	remove)
		remove_service
		;;
	start | stop | restart)
		"${SUDO[@]}" systemctl "$1" "${SERVICE_NAME}"
		;;
	status)
		"${SUDO[@]}" systemctl status "${SERVICE_NAME}" --no-pager
		;;
	log)
		"${SUDO[@]}" journalctl -u "${SERVICE_NAME}" -f
		;;
	-h | --help | help)
		usage
		;;
	*)
		usage >&2
		exit 2
		;;
esac
