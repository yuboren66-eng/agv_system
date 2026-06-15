#!/bin/bash
# start_api.sh — 启动所有 FastCGI 接口进程
#
# 用法：
#   chmod +x start_api.sh
#   ./start_api.sh          # 启动
#   ./start_api.sh stop     # 停止
#
# 生产环境用 systemd 替代此脚本，见 agv_api.service 模板。

set -e

API_DIR="$(cd "$(dirname "$0")" && pwd)"
SOCK_DIR="/run/agv"
PID_DIR="/run/agv"
LOG_DIR="/var/log/agv"

mkdir -p "$SOCK_DIR" "$LOG_DIR"

start() {
    echo "[start_api] starting FastCGI processes..."

    spawn-fcgi \
        -s "$SOCK_DIR/status.sock" \
        -P "$PID_DIR/status.pid" \
        -n \
        -- "$API_DIR/Status" >> "$LOG_DIR/status.log" 2>&1 &

    spawn-fcgi \
        -s "$SOCK_DIR/task.sock" \
        -P "$PID_DIR/task.pid" \
        -n \
        -- "$API_DIR/Task" >> "$LOG_DIR/task.log" 2>&1 &

    spawn-fcgi \
        -s "$SOCK_DIR/topo.sock" \
        -P "$PID_DIR/topo.pid" \
        -n \
        -- "$API_DIR/Topo" >> "$LOG_DIR/topo.log" 2>&1 &

    sleep 0.5
    chmod 666 "$SOCK_DIR"/*.sock
    echo "[start_api] all started. sockets in $SOCK_DIR"
}

stop() {
    echo "[start_api] stopping..."
    for f in "$PID_DIR"/*.pid; do
        [ -f "$f" ] && kill "$(cat "$f")" 2>/dev/null && rm -f "$f"
    done
    rm -f "$SOCK_DIR"/*.sock
    echo "[start_api] stopped."
}

case "${1:-start}" in
    start) start ;;
    stop)  stop  ;;
    restart) stop; sleep 1; start ;;
    *) echo "usage: $0 {start|stop|restart}"; exit 1 ;;
esac