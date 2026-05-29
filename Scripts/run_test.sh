#!/bin/bash
# ============================================================
#  NetBus SCPI-over-TCP 自动化测试 (Conda 环境)
#  用法: ./run_test.sh [设备IP, 默认 192.168.1.10]
# ============================================================

DEVICE_IP=${1:-192.168.1.10}
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPORT_DIR="$SCRIPT_DIR/reports"
mkdir -p "$REPORT_DIR"

echo ""
echo "============================================================"
echo "  NetBus SCPI-over-TCP 自动化测试"
echo "  目标: $DEVICE_IP:5025"
echo "============================================================"
echo ""

# --- 检查 conda ---
if ! command -v conda &>/dev/null; then
    echo "[FAIL] 未找到 conda"
    exit 1
fi

# --- 创建/更新环境 ---
ENV_NAME="netbus-test"
if ! conda env list | grep -q "$ENV_NAME"; then
    echo "[INFO] 创建 conda 环境: $ENV_NAME"
    conda env create -f "$SCRIPT_DIR/environment.yml" -q || exit 1
fi

# --- 运行 ---
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
REPORT="$REPORT_DIR/report_$TIMESTAMP.md"

echo "[INFO] 运行测试..."
conda run -n "$ENV_NAME" python "$SCRIPT_DIR/scpi_test.py" --host "$DEVICE_IP" --output "$REPORT"
RC=$?

if [ $RC -eq 0 ]; then
    echo ""
    echo "============================================================"
    echo "  测试完成 - 全部通过"
    echo "  报告: $REPORT"
    echo "============================================================"
else
    echo ""
    echo "============================================================"
    echo "  测试完成 - 有失败项"
    echo "  报告: $REPORT"
    echo "============================================================"
fi

exit $RC
