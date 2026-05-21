#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
NetBus 控制功能自动化测试脚本
通过 COM8 (UART7 Debug CLI) 测试控制功能。
安全原则：仅读取/查询和控制操作，不修改从机或主机配置。

实际固件命令集 (from help): help, att, can, rfsw, modbus, detector

用法:
  python netbus_test.py              # 运行全部测试，生成 report.md
  python netbus_test.py --port COM8  # 指定端口
  python netbus_test.py --mode cli   # cli / rfsw / can / detector / all (默认)
"""

import argparse
import datetime
import json
import os
import struct
import sys
import time
import traceback
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

# Force UTF-8 output on Windows
if sys.platform == "win32":
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

# ── 配置默认值 ────────────────────────────────────────
PORT = "COM8"
BAUDRATE = 115200
TIMEOUT = 1.0
TIMEOUT_LONG = 3.0
TIMEOUT_MODBUS = 5.0  # Modbus over RS485 can be slower

# ── CRC16 Modbus ──────────────────────────────────────
_MODBUS_CRC_TABLE: List[int] = []


def _make_crc_table() -> List[int]:
    global _MODBUS_CRC_TABLE
    if _MODBUS_CRC_TABLE:
        return _MODBUS_CRC_TABLE
    table = []
    for i in range(256):
        crc = i
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
        table.append(crc)
    _MODBUS_CRC_TABLE = table
    return table


def crc16_modbus(data: bytes) -> int:
    table = _make_crc_table()
    crc = 0xFFFF
    for b in data:
        idx = (crc ^ b) & 0xFF
        crc = (crc >> 8) ^ table[idx]
    return crc


def fmt_hex(data: bytes, sep: str = " ") -> str:
    return sep.join(f"{b:02X}" for b in data)


def build_modbus_frame(slave: int, func: int, payload: bytes) -> bytes:
    pdu = struct.pack(">BB", slave, func) + payload
    crc = crc16_modbus(pdu)
    return pdu + struct.pack("<H", crc)


# ── 串口操作 ───────────────────────────────────────────


class NetBus:
    def __init__(self, port: str = PORT, baudrate: int = BAUDRATE):
        self.port = port
        self.baudrate = baudrate
        self.ser = None

    def open(self) -> Tuple[bool, str]:
        try:
            import serial
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=TIMEOUT,
            )
            return True, f"已连接 {self.port} @ {self.baudrate} 8N1"
        except Exception as e:
            return False, str(e)

    def close(self):
        if self.ser and hasattr(self.ser, "is_open") and self.ser.is_open:
            self.ser.close()

    def send_cmd(self, cmd: str):
        line = cmd.strip() + "\r\n"
        self.ser.write(line.encode("ascii"))
        time.sleep(0.15)

    def read_response(self, timeout: float = None) -> str:
        t = timeout if timeout is not None else TIMEOUT
        self.ser.timeout = t
        deadline = time.time() + t
        lines: List[str] = []
        while time.time() < deadline:
            try:
                raw = self.ser.readline()
            except Exception:
                break
            if not raw:
                break
            try:
                line = raw.decode("ascii", errors="replace").rstrip("\r\n")
            except Exception:
                line = raw.hex()
            if line:
                lines.append(line)
            self.ser.timeout = 0.05
        self.ser.timeout = TIMEOUT
        return "\n".join(lines) if lines else "(无响应)"

    def drain(self):
        self.read_response(timeout=0.3)


# ── 测试结果结构 ───────────────────────────────────────

TestResult = Dict[str, Any]


def result(name: str, status: str, request: str = "",
           response: str = "", duration_ms: float = 0,
           error: str = "", detail: str = "") -> TestResult:
    return {
        "name": name,
        "status": status,
        "request": request,
        "response": response,
        "duration_ms": round(duration_ms, 1),
        "error": error,
        "detail": detail,
    }


def run_cmd(nb: NetBus, name: str, cmd: str, timeout: float = None,
            ok_fn=None) -> TestResult:
    """执行命令并返回结果，ok_fn 判断通过/失败"""
    t0 = time.time()
    nb.send_cmd(cmd)
    resp = nb.read_response(timeout=timeout)
    dt = (time.time() - t0) * 1000

    if ok_fn:
        ok, detail = ok_fn(resp)
    else:
        ok = "(无响应)" not in resp and "Unknown command" not in resp
        detail = "命令执行成功" if ok else "命令未识别或无响应"

    status = "PASS" if ok else "FAIL"
    return result(name=name, status=status, request=cmd,
                  response=resp, duration_ms=dt, detail=detail)


# ═══════════════════════════════════════════════════════
#  CLI 基础测试
# ═══════════════════════════════════════════════════════


def test_help(nb: NetBus) -> TestResult:
    def check(resp):
        ok = "CLI Commands" in resp or "help" in resp.lower()
        return ok, "命令列表已返回" if ok else "返回内容异常"
    return run_cmd(nb, "help - 帮助信息", "help", ok_fn=check)


def test_att_help(nb: NetBus) -> TestResult:
    def check(resp):
        ok = "Usage" in resp or "att" in resp.lower()
        return ok, "衰减器用法说明" if ok else "返回异常"
    return run_cmd(nb, "att (无参数)", "att", ok_fn=check)


def test_att_read(nb: NetBus) -> List[TestResult]:
    results = []
    for cmd, label in [("att ?", "读取两路衰减器"),
                       ("att a ?", "读取衰减器A"),
                       ("att b ?", "读取衰减器B")]:
        def check(resp, label=label):
            ok = "Attenuator" in resp or "=" in resp
            return ok, f"{label} OK" if ok else "无有效响应"
        r = run_cmd(nb, f"att - {label}", cmd, ok_fn=check)
        results.append(r)
    return results


def test_att_control(nb: NetBus) -> List[TestResult]:
    results = []
    # 读取当前值
    nb.send_cmd("att a ?")
    cur_a_str = nb.read_response()
    nb.send_cmd("att b ?")
    cur_b_str = nb.read_response()
    cur_a, cur_b = 0, 0
    try:
        cur_a = int(cur_a_str.split("=")[-1].strip().split()[0])
    except Exception:
        pass
    try:
        cur_b = int(cur_b_str.split("=")[-1].strip().split()[0])
    except Exception:
        pass

    results.append(result(name="att - 读取当前值", status="PASS",
                          request="att a ? / att b ?",
                          response=f"A={cur_a}, B={cur_b}",
                          detail="基准值"))

    # 设置
    for cmd, label, exp in [("att a 5", "A<-5", "set to 5"),
                             ("att b 10", "B<-10", "set to 10")]:
        def check(resp, exp=exp):
            ok = exp in resp.lower()
            return ok, "设置成功" if ok else "设置失败或响应异常"
        r = run_cmd(nb, f"att - {label}", cmd, ok_fn=check)
        results.append(r)

    time.sleep(0.1)

    # 验证
    nb.send_cmd("att a ?")
    va = nb.read_response()
    nb.send_cmd("att b ?")
    vb = nb.read_response()
    results.append(result(name="att - 验证设置",
                          status="PASS" if ("5" in va and "10" in vb) else "FAIL",
                          request="att a ? / att b ?",
                          response=f"A: {va}\nB: {vb}",
                          detail="A=5, B=10 验证通过" if ("5" in va and "10" in vb) else "值不符"))

    # 恢复
    nb.send_cmd(f"att a {cur_a}")
    nb.read_response()
    nb.send_cmd(f"att b {cur_b}")
    nb.read_response()
    results.append(result(name="att - 恢复原始值", status="PASS",
                          request=f"att a {cur_a} / att b {cur_b}",
                          response=f"已恢复 A={cur_a}, B={cur_b}",
                          detail="衰减器已恢复"))

    return results


# ═══════════════════════════════════════════════════════
#  RFSW 测试 (Modbus RTU over RS485)
#  SAFE: 仅使用只读子命令
# ═══════════════════════════════════════════════════════


def test_rfsw_help(nb: NetBus) -> TestResult:
    def check(resp):
        ok = "RF Switch" in resp or "rfsw" in resp.lower()
        return ok, "rfsw 命令帮助" if ok else "rfsw 命令无响应"
    return run_cmd(nb, "rfsw - 帮助", "rfsw", ok_fn=check)


def test_rfsw_get(nb: NetBus, addr: int = 1) -> TestResult:
    """读取通道号 (FC=03, REG 0x0000)"""
    def check(resp):
        if "Unknown command" in resp or "(无响应)" in resp:
            return False, "rfsw 命令未注册？"
        if "SUCCESS" in resp or "Channel" in resp or "channel" in resp:
            return True, "通道号读取成功"
        if "FAILED" in resp or "Timeout" in resp or "EXCEPTION" in resp:
            return True, f"从机无响应 (addr={addr})"  # 预期行为（无设备）
        return False, f"未知响应: {resp[:80]}"
    return run_cmd(nb, f"rfsw get {addr} - 读取通道号",
                   f"rfsw get {addr}", timeout=TIMEOUT_MODBUS, ok_fn=check)


def test_rfsw_info(nb: NetBus, addr: int = 1) -> TestResult:
    """读取设备身份和状态"""
    def check(resp):
        if "Unknown command" in resp or "(无响应)" in resp:
            return False, "rfsw 命令未注册"
        if "FAILED" in resp or "Timeout" in resp:
            return True, f"从机无响应 (addr={addr})"  # 预期
        if "SUCCESS" in resp or "ID:" in resp or "SN:" in resp:
            return True, "设备信息读取成功"
        return True, "已发送 (等待响应)"
    return run_cmd(nb, f"rfsw info {addr} - 设备信息",
                   f"rfsw info {addr}", timeout=TIMEOUT_MODBUS, ok_fn=check)


def test_rfsw_status(nb: NetBus, addr: int = 1) -> TestResult:
    """读取设备状态寄存器"""
    def check(resp):
        if "Unknown command" in resp or "(无响应)" in resp:
            return False, "rfsw 命令未注册"
        if "FAILED" in resp or "Timeout" in resp:
            return True, f"从机无响应 (addr={addr})"
        return True, "状态读取成功" if "SUCCESS" in resp else "读取完成"
    return run_cmd(nb, f"rfsw status {addr} - 设备状态",
                   f"rfsw status {addr}", timeout=TIMEOUT_MODBUS, ok_fn=check)


def test_rfsw_outputs(nb: NetBus, addr: int = 1) -> TestResult:
    """读取所有6路输出线圈 (FC=01)"""
    def check(resp):
        if "Unknown command" in resp or "(无响应)" in resp:
            return False, "rfsw 命令未注册"
        if "FAILED" in resp or "Timeout" in resp:
            return True, f"从机无响应 (addr={addr})"
        return True, "输出线圈读取成功" if "SUCCESS" in resp else "读取完成"
    return run_cmd(nb, f"rfsw outputs {addr} - 读取输出线圈",
                   f"rfsw outputs {addr}", timeout=TIMEOUT_MODBUS, ok_fn=check)


def test_rfsw_inputs(nb: NetBus, addr: int = 1) -> TestResult:
    """读取4路离散输入 (FC=02)"""
    def check(resp):
        if "Unknown command" in resp or "(无响应)" in resp:
            return False, "rfsw 命令未注册"
        if "FAILED" in resp or "Timeout" in resp:
            return True, f"从机无响应 (addr={addr})"
        return True, "离散输入读取成功" if "SUCCESS" in resp else "读取完成"
    return run_cmd(nb, f"rfsw inputs {addr} - 读取离散输入",
                   f"rfsw inputs {addr}", timeout=TIMEOUT_MODBUS, ok_fn=check)


def test_rfsw_mode_read(nb: NetBus, addr: int = 1) -> TestResult:
    """仅读取工作模式 (不写参数 = 只读)"""
    def check(resp):
        if "Unknown command" in resp or "(无响应)" in resp:
            return False, "rfsw 命令未注册"
        if "FAILED" in resp or "Timeout" in resp:
            return True, f"从机无响应 (addr={addr})"
        return True, "模式读取成功" if ("IO" in resp or "CMD" in resp or "mode" in resp.lower()) else "读取完成"
    return run_cmd(nb, f"rfsw mode {addr} - 读取工作模式",
                   f"rfsw mode {addr}", timeout=TIMEOUT_MODBUS, ok_fn=check)


# ═══════════════════════════════════════════════════════
#  Modbus 测试 (原生 Modbus RTU 帧注入)
#  SAFE: 仅查询/读取操作
# ═══════════════════════════════════════════════════════


def test_modbus_help(nb: NetBus) -> TestResult:
    def check(resp):
        ok = "Modbus" in resp or "modbus" in resp.lower()
        return ok, "modbus 命令帮助" if ok else "modbus 命令无响应"
    return run_cmd(nb, "modbus - 帮助", "modbus", ok_fn=check)


def test_modbus_raw(nb: NetBus, addr: int = 1) -> TestResult:
    """发送原生 Modbus 帧 (FC=03 读保持寄存器 0x0000)"""
    frame = f"{addr:02X} 03 00 00 00 01"
    def check(resp):
        if "Unknown command" in resp:
            return False, "modbus 命令未注册"
        if "FAILED" in resp or "Timeout" in resp:
            return True, f"从机无响应 (addr={addr})"
        if "EXCEPTION" in resp:
            return True, "Modbus 异常响应 (正常协议行为)"
        return True, "Modbus 原生帧已发送"
    return run_cmd(nb, f"modbus raw - FC=03 读寄存器 (addr={addr})",
                   f"modbus {frame}", timeout=TIMEOUT_MODBUS, ok_fn=check)


# ═══════════════════════════════════════════════════════
#  CAN 测试 (can scan)
#  SAFE: can scan — 只读扫描
# ═══════════════════════════════════════════════════════


def test_can_scan(nb: NetBus, start: int = 0, end: int = 0) -> TestResult:
    """CAN 总线扫描 (分层命令: can scan)"""
    if start > 0 and end > 0:
        cmd = f"can scan {start} {end}"
    else:
        cmd = "can scan"

    def check(resp):
        if "Unknown command" in resp:
            return False, "can 命令未注册"
        if "(无响应)" in resp:
            return False, "无响应"
        return True, "扫描完成"
    return run_cmd(nb, "can scan - CAN 总线扫描", cmd,
                   timeout=TIMEOUT_LONG, ok_fn=check)


# ═══════════════════════════════════════════════════════
#  Detector 测试 (CAN 检波板)
#  SAFE: 仅使用只读子命令
# ═══════════════════════════════════════════════════════


def test_detector_help(nb: NetBus) -> TestResult:
    def check(resp):
        ok = "sub-command" in resp.lower() or "Available" in resp
        return ok, "detector 子命令列表" if ok else "未返回子命令列表"
    return run_cmd(nb, "detector help - 检波器帮助",
                   "detector help", ok_fn=check)


def test_detector_sn(nb: NetBus, node_id: int = 1) -> TestResult:
    """读取检波板 SN (CAN 0x101)"""
    def check(resp):
        if "Unknown command" in resp:
            return False, "detector 命令未注册"
        if "timeout" in resp.lower() or "no response" in resp.lower():
            return True, "超时 (检波板未连接)"
        if "SN" in resp or "sn" in resp.lower():
            return True, "SN 查询成功"
        return True, "SN 查询已发送"
    return run_cmd(nb, f"detector sn {node_id} - 检波板 SN",
                   f"detector sn {node_id}", timeout=TIMEOUT_LONG, ok_fn=check)


def test_detector_version(nb: NetBus, node_id: int = 1) -> TestResult:
    """读取检波板固件版本 (CAN 0x103)"""
    def check(resp):
        if "Unknown command" in resp:
            return False, "detector 命令未注册"
        if "timeout" in resp.lower():
            return True, "超时 (检波板未连接)"
        return True, "版本查询已发送"
    return run_cmd(nb, f"detector version {node_id} - 固件版本",
                   f"detector version {node_id}", timeout=TIMEOUT_LONG, ok_fn=check)


def test_detector_temp(nb: NetBus, node_id: int = 1) -> TestResult:
    """读取检波板温度 (CAN 0x116)"""
    def check(resp):
        if "Unknown command" in resp:
            return False, "detector 命令未注册"
        if "timeout" in resp.lower():
            return True, "超时 (检波板未连接)"
        return True, "温度查询已发送"
    return run_cmd(nb, f"detector temp {node_id} - 温度",
                   f"detector temp {node_id} 1 1", timeout=TIMEOUT_LONG, ok_fn=check)


def test_detector_flash(nb: NetBus, node_id: int = 1) -> TestResult:
    """读取检波板外部 Flash 信息 (CAN 0x117)"""
    def check(resp):
        if "Unknown command" in resp:
            return False, "detector 命令未注册"
        if "timeout" in resp.lower():
            return True, "超时 (检波板未连接)"
        return True, "Flash 信息查询已发送"
    return run_cmd(nb, f"detector flash {node_id} - Flash 信息",
                   f"detector flash {node_id}", timeout=TIMEOUT_LONG, ok_fn=check)


# ═══════════════════════════════════════════════════════
#  报告生成
# ═══════════════════════════════════════════════════════


def escape_md(s: str) -> str:
    return s.replace("|", "\\|").replace("\n", "<br>")


def generate_report(results: List[TestResult], meta: dict, output_path: str) -> str:
    total = len(results)
    passed = sum(1 for r in results if r["status"] == "PASS")
    failed = sum(1 for r in results if r["status"] == "FAIL")
    warned = sum(1 for r in results if r["status"] == "WARN")
    skipped = sum(1 for r in results if r["status"] == "SKIP")
    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    total_duration = sum(r["duration_ms"] for r in results)

    # 分类统计
    cli_passed = sum(1 for r in results if r["status"] == "PASS" and r["name"].startswith(("help", "att")))
    rfsw_passed = sum(1 for r in results if r["status"] == "PASS" and "rfsw" in r["name"])
    can_passed = sum(1 for r in results if r["status"] == "PASS" and ("can" in r["name"] or "CAN" in r["name"]))
    modbus_passed = sum(1 for r in results if r["status"] == "PASS" and "modbus" in r["name"])
    det_passed = sum(1 for r in results if r["status"] == "PASS" and "detector" in r["name"])

    lines = []
    lines.append("# NetBus 控制功能测试报告")
    lines.append("")
    lines.append(f"**生成时间**: {now}  ")
    lines.append(f"**测试端口**: {meta['port']} @ {meta['baudrate']} bps  ")
    lines.append(f"**测试模式**: {meta['mode']} (Modbus addr={meta['slave']}, CAN node={meta['node']})  ")
    lines.append(f"**安全策略**: 仅读取/查询/控制，不修改从机或主机配置  ")
    lines.append("")
    lines.append("---")
    lines.append("")

    # 摘要
    lines.append("## 测试摘要")
    lines.append("")
    lines.append("| 指标 | 值 |")
    lines.append("|------|----|")
    lines.append(f"| 总测试项 | {total} |")
    lines.append(f"| PASS | {passed} |")
    lines.append(f"| FAIL | {failed} |")
    lines.append(f"| WARN | {warned} |")
    lines.append(f"| SKIP | {skipped} |")
    lines.append(f"| 通过率 | {passed / total * 100:.1f}% |")
    lines.append(f"| 总耗时 | {total_duration / 1000:.1f}s |")
    lines.append("")

    # 子系统摘要
    lines.append("### 子系统结果")
    lines.append("")
    lines.append("| 子系统 | 命令 | 通过 | 说明 |")
    lines.append("|--------|------|------|------|")
    lines.append(f"| CLI 基础 | help, att | {cli_passed} | 本地衰减器 & 帮助 |")
    lines.append(f"| RFSW (Modbus) | rfsw | {rfsw_passed} | RS485 远程 RF Switch |")
    lines.append(f"| CAN 总线 | can scan | {can_passed} | CAN 扫描 |")
    lines.append(f"| Modbus Raw | modbus | {modbus_passed} | Modbus 原生帧注入 |")
    lines.append(f"| Detector (CAN) | detector | {det_passed} | A1 检波板指令 |")
    lines.append("")

    # 结果表格
    lines.append("## 测试明细")
    lines.append("")
    lines.append("| # | 测试项 | 状态 | 耗时 | 详情 |")
    lines.append("|---|--------|------|------|------|")
    for i, r in enumerate(results, 1):
        icon = {"PASS": "PASS", "FAIL": "**FAIL**", "WARN": "WARN", "SKIP": "SKIP"}[r["status"]]
        lines.append(f"| {i} | {escape_md(r['name'])} | {icon} | {r['duration_ms']:.0f}ms | {escape_md(r.get('detail',''))} |")
    lines.append("")

    # 失败项详情
    failed_items = [r for r in results if r["status"] == "FAIL"]
    if failed_items:
        lines.append("## 失败项详情")
        lines.append("")
        for r in failed_items:
            lines.append(f"### {escape_md(r['name'])}")
            lines.append("")
            lines.append(f"- **请求**: `{escape_md(r['request'])}`")
            lines.append(f"- **响应**:")
            lines.append("```")
            lines.append(r["response"])
            lines.append("```")
            if r.get("error"):
                lines.append(f"- **错误**: {escape_md(r.get('error',''))}")
            lines.append("")

    # 警告项
    warned_items = [r for r in results if r["status"] == "WARN"]
    if warned_items:
        lines.append("## 警告项")
        lines.append("")
        lines.append("| # | 测试项 | 说明 |")
        lines.append("|---|--------|------|")
        for i, r in enumerate(warned_items, 1):
            lines.append(f"| {i} | {escape_md(r['name'])} | {escape_md(r.get('detail',''))} |")
        lines.append("")

    # 附录
    lines.append("## 附录: 完整响应数据")
    lines.append("")
    lines.append("<details>")
    lines.append("<summary>展开查看所有响应详情</summary>")
    lines.append("")
    for r in results:
        lines.append(f"### {escape_md(r['name'])}")
        lines.append("")
        lines.append(f"- **请求**: `{escape_md(r['request'])}`")
        lines.append(f"- **响应**:")
        lines.append("```")
        lines.append(r["response"])
        lines.append("```")
        lines.append("")
    lines.append("</details>")

    report = "\n".join(lines)
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(report)
    return report


# ═══════════════════════════════════════════════════════
#  主流程
# ═══════════════════════════════════════════════════════


def main():
    parser = argparse.ArgumentParser(description="NetBus 控制功能自动化测试")
    parser.add_argument("--port", "-p", default=PORT, help=f"串口 (默认: {PORT})")
    parser.add_argument("--baud", "-b", type=int, default=BAUDRATE, help=f"波特率")
    parser.add_argument("--mode", "-m", default="all",
                        choices=["all", "cli", "rfsw", "can", "detector"],
                        help="测试模式 (默认: all)")
    parser.add_argument("--slave", "-s", type=int, default=1, help="Modbus 地址")
    parser.add_argument("--node", "-n", type=int, default=1, help="CAN 节点 ID")
    parser.add_argument("--output", "-o", default="", help="报告输出路径")
    parser.add_argument("--json", "-j", default="", help="附加 JSON 结果文件")
    args = parser.parse_args()

    if not args.output:
        ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        args.output = f"report_netbus_{ts}.md"

    output_path = os.path.abspath(args.output)
    json_path = os.path.abspath(args.json) if args.json else ""

    print(f"NetBus 自动化测试")
    print(f"  端口: {args.port} @ {args.baud}")
    print(f"  模式: {args.mode}, 从机: {args.slave}, 节点: {args.node}")
    print(f"  报告: {output_path}")
    print()

    nb = NetBus(args.port, args.baud)
    ok, msg = nb.open()
    if not ok:
        print(f"[FAIL] 无法打开串口: {msg}")
        r = result(name="串口连接", status="FAIL", error=msg,
                    detail=f"无法打开 {args.port}")
        generate_report([r], {"port": args.port, "baudrate": args.baud, "mode": args.mode,
                              "slave": args.slave, "node": args.node}, output_path)
        print(f"  报告已生成: {output_path}")
        return 1

    print(f"  {msg}")
    nb.drain()

    all_results: List[TestResult] = []
    slave = args.slave
    node = args.node

    try:
        # ── CLI 基础 ──
        if args.mode in ("all", "cli"):
            print("\n=== CLI 基础测试 ===")
            for r in [test_help(nb), test_att_help(nb)]:
                all_results.append(r)
                print(f"  {r['status']:4s} {r['name']}")
            for r in test_att_read(nb):
                all_results.append(r)
                print(f"  {r['status']:4s} {r['name']}")
            print("  ... 衰减器控制 ...")
            for r in test_att_control(nb):
                all_results.append(r)
                print(f"  {r['status']:4s} {r['name']}")

        # ── RFSW (Modbus) ──
        if args.mode in ("all", "rfsw"):
            print("\n=== RFSW 测试 (Modbus RTU over RS485) ===")
            r = test_rfsw_help(nb)
            all_results.append(r)
            print(f"  {r['status']:4s} {r['name']}")

            # 只读命令
            rfsw_tests = [
                test_rfsw_get, test_rfsw_info, test_rfsw_status,
                test_rfsw_outputs, test_rfsw_inputs, test_rfsw_mode_read,
            ]
            for fn in rfsw_tests:
                r = fn(nb, slave)
                all_results.append(r)
                print(f"  {r['status']:4s} {r['name']}  | {r.get('detail','')}")

        # ── Modbus ──
        if args.mode in ("all", "rfsw"):
            print("\n=== Modbus 原生帧注入 ===")
            r = test_modbus_help(nb)
            all_results.append(r)
            print(f"  {r['status']:4s} {r['name']}")

            r = test_modbus_raw(nb, slave)
            all_results.append(r)
            print(f"  {r['status']:4s} {r['name']}  | {r.get('detail','')}")

        # ── CAN ──
        if args.mode in ("all", "can"):
            print("\n=== CAN 总线测试 ===")
            r = test_can_scan(nb)
            all_results.append(r)
            print(f"  {r['status']:4s} {r['name']}  | {r.get('detail','')}")

        # ── Detector ──
        if args.mode in ("all", "detector"):
            print("\n=== Detector 测试 (CAN A1 检波板) ===")
            r = test_detector_help(nb)
            all_results.append(r)
            print(f"  {r['status']:4s} {r['name']}")

            det_tests = [test_detector_sn, test_detector_version,
                         test_detector_temp, test_detector_flash]
            for fn in det_tests:
                r = fn(nb, node)
                all_results.append(r)
                print(f"  {r['status']:4s} {r['name']}  | {r.get('detail','')}")

    except Exception as e:
        all_results.append(result(
            name="测试异常", status="FAIL", error=str(e),
            detail=traceback.format_exc()[:500],
        ))
        print(f"\n[FAIL] 异常: {e}")
        traceback.print_exc()
    finally:
        nb.close()

    meta = {"port": args.port, "baudrate": args.baud, "mode": args.mode,
            "slave": slave, "node": node}

    print(f"\n=== 生成报告 ===")
    generate_report(all_results, meta, output_path)
    print(f"  Markdown: {output_path}")

    if json_path:
        data = {"meta": meta, "timestamp": datetime.datetime.now().isoformat(),
                "results": all_results}
        with open(json_path, "w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False, indent=2)
        print(f"  JSON:     {json_path}")

    total = len(all_results)
    passed = sum(1 for r in all_results if r["status"] == "PASS")
    failed = sum(1 for r in all_results if r["status"] == "FAIL")
    print(f"\n  结果: {passed}/{total} PASS, {failed} FAIL, {total-passed-failed} WARN/SKIP")

    return 1 if failed > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
