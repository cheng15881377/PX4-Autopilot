#!/usr/bin/env bash

set -euo pipefail

# ======================== 常用配置：需要时改这里 ========================
DEFAULT_TARGET_IP="192.168.0.138"
DEFAULT_TARGET_USER="cat"
BUILD_CONTAINER="px4-rk3588-build"
CONTAINER_SOURCE_DIR="/work"
PX4_TARGET="embedfire_rk3588_default"
# ======================================================================

usage()
{
	cat <<EOF
用法：
  $0 [鲁班猫IP] [SSH用户名]

示例：
  $0
  $0 192.168.0.138
  $0 192.168.0.138 cat

默认值：
  IP:       ${DEFAULT_TARGET_IP}
  用户名:   ${DEFAULT_TARGET_USER}
  容器:     ${BUILD_CONTAINER}
  编译目标: ${PX4_TARGET}
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
	usage
	exit 0
fi

if (( $# > 2 )); then
	usage >&2
	exit 2
fi

TARGET_IP="${1:-${DEFAULT_TARGET_IP}}"
TARGET_USER="${2:-${DEFAULT_TARGET_USER}}"

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build/${PX4_TARGET}"
STARTUP_CONFIG="${SCRIPT_DIR}/posix-configs/embedfire/rk3588_mc.config"
RUN_SCRIPT="${SCRIPT_DIR}/posix-configs/embedfire/run_px4.sh"
STOP_SCRIPT="${SCRIPT_DIR}/posix-configs/embedfire/stop_px4.sh"
AUTOSTART_SCRIPT="${SCRIPT_DIR}/posix-configs/embedfire/install_px4_autostart.sh"
REMOTE_DIR="/home/${TARGET_USER}/px4"

run_docker()
{
	if docker info >/dev/null 2>&1; then
		docker "$@"
		return
	fi

	if command -v sg >/dev/null 2>&1; then
		local command
		printf -v command '%q ' docker "$@"
		sg docker -c "${command}"
		return
	fi

	echo "错误：当前用户无法访问 Docker，请将用户加入 docker 组后重新登录。" >&2
	exit 1
}

for command in docker rsync; do
	if ! command -v "${command}" >/dev/null 2>&1; then
		echo "错误：没有找到命令 ${command}" >&2
		exit 1
	fi
done

if ! run_docker inspect "${BUILD_CONTAINER}" >/dev/null 2>&1; then
	echo "错误：Docker 容器 ${BUILD_CONTAINER} 不存在。" >&2
	exit 1
fi

if [[ "$(run_docker inspect --format '{{.State.Running}}' "${BUILD_CONTAINER}")" != "true" ]]; then
	echo "启动 Docker 容器：${BUILD_CONTAINER}"
	run_docker start "${BUILD_CONTAINER}" >/dev/null
fi

if ! run_docker exec "${BUILD_CONTAINER}" test -f "${CONTAINER_SOURCE_DIR}/Makefile"; then
	echo "错误：容器内没有找到 ${CONTAINER_SOURCE_DIR}/Makefile，请检查仓库挂载路径。" >&2
	exit 1
fi

echo "[1/2] 在 Docker 中编译 ${PX4_TARGET}"
printf -v build_command 'cd %q && make %q' "${CONTAINER_SOURCE_DIR}" "${PX4_TARGET}"
run_docker exec "${BUILD_CONTAINER}" bash -lc "${build_command}"

if [[ ! -x "${BUILD_DIR}/bin/px4" || ! -d "${BUILD_DIR}/etc" || ! -f "${STARTUP_CONFIG}"
	|| ! -x "${RUN_SCRIPT}" || ! -x "${STOP_SCRIPT}" || ! -x "${AUTOSTART_SCRIPT}" ]]; then
	echo "错误：编译产物不完整，停止上传。" >&2
	exit 1
fi

echo "[2/2] 上传到 ${TARGET_USER}@${TARGET_IP}:${REMOTE_DIR}"
echo "SSH 要求输入密码时，请输入鲁班猫 ${TARGET_USER} 用户的密码。"
rsync -arh --progress \
	"${BUILD_DIR}/bin" \
	"${STARTUP_CONFIG}" \
	"${RUN_SCRIPT}" \
	"${STOP_SCRIPT}" \
	"${AUTOSTART_SCRIPT}" \
	"${BUILD_DIR}/etc" \
	"${TARGET_USER}@${TARGET_IP}:${REMOTE_DIR}"

echo
echo "编译和上传完成。"
echo "鲁班猫启动命令："
echo "  cd ${REMOTE_DIR}"
echo "  ./run_px4.sh"
echo "鲁班猫完全关闭命令："
echo "  ./stop_px4.sh"
echo "首次安装开机自动启动："
echo "  ./install_px4_autostart.sh"
