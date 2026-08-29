#!/usr/bin/env bash

set -euo pipefail

# PX4 固定运行在 RK3588 逻辑 CPU 7，需要时可以修改这里。
PX4_CPU="7"
PX4_INSTANCE="0"

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PX4_BINARY="${SCRIPT_DIR}/bin/px4"
PX4_CONFIG="${SCRIPT_DIR}/rk3588_mc.config"
PX4_LOCK_FILE="/tmp/px4_lock-${PX4_INSTANCE}"
PX4_SOCKET_FILE="/tmp/px4-sock-${PX4_INSTANCE}"

if [[ ! -x "${PX4_BINARY}" ]]; then
	echo "错误：没有找到可执行文件 ${PX4_BINARY}" >&2
	exit 1
fi

if [[ ! -f "${PX4_CONFIG}" ]]; then
	echo "错误：没有找到启动配置 ${PX4_CONFIG}" >&2
	exit 1
fi

if (( EUID == 0 )); then
	SUDO=()
else
	sudo -v
	SUDO=(sudo -n)
fi

active_px4_pids()
{
	local pid state

	while read -r pid; do
		[[ -n "${pid}" ]] || continue
		state="$("${SUDO[@]}" ps -o stat= -p "${pid}" 2>/dev/null || true)"
		state="${state//[[:space:]]/}"

		# A zombie has already exited and cannot hold PX4 resources or locks.
		if [[ -n "${state}" && "${state:0:1}" != "Z" && "${state:0:1}" != "X" ]]; then
			echo "${pid}"
		fi
	done < <("${SUDO[@]}" pgrep -x px4 2>/dev/null || true)
}

mapfile -t ACTIVE_PIDS < <(active_px4_pids)

if (( ${#ACTIVE_PIDS[@]} > 0 )); then
	echo "PX4 已经在运行。请先执行 ${SCRIPT_DIR}/stop_px4.sh" >&2
	echo "活动进程：${ACTIVE_PIDS[*]}" >&2
	exit 1
fi

# PX4 may previously have been started as a different user. Linux protects
# files owned by another user in /tmp, so stale IPC files must be removed
# before the root PX4 process creates them again.
"${SUDO[@]}" rm -f -- "${PX4_LOCK_FILE}" "${PX4_SOCKET_FILE}"

cd "${SCRIPT_DIR}"

echo "启动 PX4：CPU ${PX4_CPU}，工作目录 ${SCRIPT_DIR}"
echo "正常退出请按 Ctrl+C；不要使用 Ctrl+Z。"
exec "${SUDO[@]}" taskset -c "${PX4_CPU}" "${PX4_BINARY}" -i "${PX4_INSTANCE}" -s "${PX4_CONFIG}"
