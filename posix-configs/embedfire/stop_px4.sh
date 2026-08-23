#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PX4_SHUTDOWN="${SCRIPT_DIR}/bin/px4-shutdown"

if (( EUID == 0 )); then
	SUDO=()
else
	sudo -v
	SUDO=(sudo -n)
fi

px4_pids()
{
	"${SUDO[@]}" pgrep -x px4 2>/dev/null || true
}

active_px4_pids()
{
	local pid state

	while read -r pid; do
		[[ -n "${pid}" ]] || continue
		state="$("${SUDO[@]}" ps -o stat= -p "${pid}" 2>/dev/null || true)"
		state="${state//[[:space:]]/}"

		if [[ -n "${state}" && "${state:0:1}" != "Z" && "${state:0:1}" != "X" ]]; then
			echo "${pid}"
		fi
	done < <(px4_pids)
}

zombie_px4_pids()
{
	local pid state

	while read -r pid; do
		[[ -n "${pid}" ]] || continue
		state="$("${SUDO[@]}" ps -o stat= -p "${pid}" 2>/dev/null || true)"
		state="${state//[[:space:]]/}"

		if [[ -n "${state}" && "${state:0:1}" == "Z" ]]; then
			echo "${pid}"
		fi
	done < <(px4_pids)
}

wake_supervisor()
{
	local px4_pid="$1"
	local parent_pid parent_name
	parent_pid="$("${SUDO[@]}" ps -o ppid= -p "${px4_pid}" 2>/dev/null || true)"
	parent_pid="${parent_pid//[[:space:]]/}"

	if [[ -n "${parent_pid}" ]]; then
		parent_name="$("${SUDO[@]}" ps -o comm= -p "${parent_pid}" 2>/dev/null || true)"
		parent_name="${parent_name//[[:space:]]/}"

		if [[ "${parent_name}" == "sudo" || "${parent_name}" == "taskset" ]]; then
			"${SUDO[@]}" kill -CONT "${parent_pid}" 2>/dev/null || true
		fi
	fi
}

mapfile -t ACTIVE_PIDS < <(active_px4_pids)
mapfile -t ZOMBIE_PIDS < <(zombie_px4_pids)

if (( ${#ACTIVE_PIDS[@]} == 0 )); then
	if (( ${#ZOMBIE_PIDS[@]} > 0 )); then
		for pid in "${ZOMBIE_PIDS[@]}"; do
			wake_supervisor "${pid}"
		done

		echo "PX4 已停止；检测到的僵尸进程不再占用硬件，已通知父进程回收：${ZOMBIE_PIDS[*]}"
	else
		echo "PX4 没有运行。"
	fi

	exit 0
fi

echo "正在关闭 PX4……"

# 先唤醒被 Ctrl+Z 暂停的 PX4 及其 sudo/taskset 监护进程。
for pid in "${ACTIVE_PIDS[@]}"; do
	wake_supervisor "${pid}"
	"${SUDO[@]}" kill -CONT "${pid}" 2>/dev/null || true
done

# 优先通过PX4自己的客户端请求关闭。该路径会先通知logger保存日志，再由
# PX4内部的shutdown worker退出进程，避免SIGTERM先关闭uORB所造成的一串
# Accel/Gyro/Baro/Mag TIMEOUT日志。
shutdown_requested=false

if [[ -x "${PX4_SHUTDOWN}" ]] && timeout 3 "${SUDO[@]}" "${PX4_SHUTDOWN}"; then
	shutdown_requested=true
else
	echo "PX4内部关闭命令不可用，改用SIGTERM。" >&2

	for pid in "${ACTIVE_PIDS[@]}"; do
		"${SUDO[@]}" kill -TERM "${pid}" 2>/dev/null || true
	done
fi

# PX4内部shutdown worker自身有5秒保护超时，这里多留3秒用于logger收尾。
for ((attempt = 0; attempt < 80; attempt++)); do
	mapfile -t ACTIVE_PIDS < <(active_px4_pids)

	if (( ${#ACTIVE_PIDS[@]} == 0 )); then
		echo "PX4 已完全关闭。"
		exit 0
	fi

	sleep 0.1
done

if [[ "${shutdown_requested}" == "true" ]]; then
	echo "PX4内部关闭请求在8秒内没有完成，执行强制关闭。" >&2
else
	echo "PX4在8秒内没有退出，执行强制关闭。" >&2
fi
for pid in "${ACTIVE_PIDS[@]}"; do
	"${SUDO[@]}" kill -KILL "${pid}" 2>/dev/null || true
done

sleep 0.5
mapfile -t ACTIVE_PIDS < <(active_px4_pids)

if (( ${#ACTIVE_PIDS[@]} > 0 )); then
	echo "错误：仍检测到活动的 PX4 进程。进程可能处于不可中断的内核等待状态：" >&2

	for pid in "${ACTIVE_PIDS[@]}"; do
		"${SUDO[@]}" ps -o pid=,ppid=,pgid=,stat=,wchan=,comm=,args= -p "${pid}" >&2 || true
	done

	echo "如果 STAT 以 D 开头，只能等待对应驱动返回；长时间不恢复时需要重启系统。" >&2
	exit 1
fi

mapfile -t ZOMBIE_PIDS < <(zombie_px4_pids)

for pid in "${ZOMBIE_PIDS[@]}"; do
	wake_supervisor "${pid}"
done

echo "PX4 已强制关闭。"
