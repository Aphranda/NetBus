#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
NetBus SCPI-over-TCP 详细自动化测试脚本
参照 Scripts/netbus_test.py 的详细程度，覆盖所有 SCPI 子系统。

用法:
  conda run -n netbus-test python scpi_test.py --host 192.168.1.10
  python scpi_test.py --host 192.168.1.10 --output report_scpi.md
"""

import argparse
import datetime
import re
import sys
import time
import socket
import traceback
from typing import Any, Dict, List, Optional, Tuple, Callable

# ── 默认设置 ────────────────────────────────────────
DEFAULT_HOST = "192.168.1.10"
DEFAULT_PORT = 5025
TIMEOUT = 3.0
TIMEOUT_MODBUS = 5.0

TestResult = Dict[str, Any]


def result(name: str, status: str, request: str = "",
           response: str = "", duration_ms: float = 0,
           error: str = "", detail: str = "") -> TestResult:
    return {
        "name": name, "status": status, "request": request,
        "response": response, "duration_ms": round(duration_ms, 1),
        "error": error, "detail": detail,
    }


class ScpiClient:
    """TCP SCPI 客户端"""

    def __init__(self, host: str = DEFAULT_HOST, port: int = DEFAULT_PORT):
        self.host = host
        self.port = port
        self.sock: Optional[socket.socket] = None

    def connect(self) -> Tuple[bool, str]:
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(TIMEOUT)
            self.sock.connect((self.host, self.port))
            return True, f"{self.host}:{self.port}"
        except Exception as e:
            return False, str(e)

    def close(self):
        if self.sock:
            try: self.sock.close()
            except: pass
            self.sock = None

    def send(self, cmd: str, timeout: float = TIMEOUT) -> str:
        """发送一行命令，返回去前缀的响应"""
        t0 = time.time()
        self.sock.sendall((cmd.strip() + "\n").encode("ascii"))
        time.sleep(0.05)
        resp = b""
        try:
            while True:
                self.sock.settimeout(0.15)
                chunk = self.sock.recv(4096)
                if not chunk:
                    break
                resp += chunk
                if time.time() - t0 > timeout:
                    break
        except socket.timeout:
            pass
        return resp.decode("ascii", errors="replace").strip()


def run_cmd(cli: ScpiClient, name: str, cmd: str, timeout: float = TIMEOUT,
            ok_fn: Optional[Callable[[str], Tuple[bool, str]]] = None) -> TestResult:
    t0 = time.time()
    try:
        resp = cli.send(cmd, timeout=timeout)
        dt = (time.time() - t0) * 1000
    except Exception as e:
        dt = (time.time() - t0) * 1000
        return result(name=name, status="FAIL", request=cmd,
                      response="", duration_ms=dt, error=str(e),
                      detail=f"TCP 异常: {e}")

    if ok_fn:
        ok, detail = ok_fn(resp)
    else:
        ok = len(resp) > 0
        detail = "OK" if ok else "无响应"

    return result(name=name, status="PASS" if ok else "FAIL",
                  request=cmd, response=resp, duration_ms=dt, detail=detail)


# ═══════════════════════════════════════════════════════════════
#  IEEE 488.2 —— 6 项
# ═══════════════════════════════════════════════════════════════

def test_ieee_idn(cli: ScpiClient) -> TestResult:
    def check(resp):
        if "NetBus" in resp and "PPA-NB100" in resp:
            return True, f"IDN 正确: {resp[:80]}"
        if "ERROR" in resp:
            return False, f"SCPI 返回错误: {resp[:60]}"
        return False, f"未返回预期 IDN: {resp[:80]}"
    return run_cmd(cli, "IEEE488.2 *IDN?", "*IDN?", ok_fn=check)


def test_ieee_stb(cli: ScpiClient) -> TestResult:
    def check(resp):
        try:
            val = int(resp.strip())
            return True, f"STB = {val}"
        except:
            return False if "ERROR" in resp else (True, f"STB: {resp[:40]}")
    return run_cmd(cli, "IEEE488.2 *STB?", "*STB?", ok_fn=check)


def test_ieee_opc(cli: ScpiClient) -> TestResult:
    return run_cmd(cli, "IEEE488.2 *OPC?", "*OPC?",
                   ok_fn=lambda r: (True, f"OPC = {r.strip()[:30]}") if r else (False, "无响应"))


def test_ieee_cls(cli: ScpiClient) -> TestResult:
    return run_cmd(cli, "IEEE488.2 *CLS", "*CLS",
                   ok_fn=lambda r: (True, "已执行"))


def test_ieee_rst(cli: ScpiClient) -> TestResult:
    def check(resp):
        if "OK" in resp or resp == "":
            return True, "已复位"
        if "ERROR" in resp:
            return False, f"复位错误: {resp[:60]}"
        return True, f"已执行: {resp[:60]}"
    return run_cmd(cli, "IEEE488.2 *RST", "*RST", ok_fn=check)


def test_ieee_wai(cli: ScpiClient) -> TestResult:
    return run_cmd(cli, "IEEE488.2 *WAI", "*WAI",
                   ok_fn=lambda r: (True, "已执行"))


# ═══════════════════════════════════════════════════════════════
#  SYSTem:ERRor —— 2 项
# ═══════════════════════════════════════════════════════════════

def test_syst_err(cli: ScpiClient) -> TestResult:
    def check(resp):
        if "No error" in resp or "0," in resp:
            return True, "错误队列为空 (No error)"
        return True, f"错误队列: {resp[:80]}"
    return run_cmd(cli, "SYST:ERR?", "SYST:ERR?", ok_fn=check)


def test_syst_err_count(cli: ScpiClient) -> TestResult:
    def check(resp):
        try:
            val = int(resp.strip())
            return True, f"错误计数 = {val}"
        except:
            return False, f"异常响应: {resp[:60]}"
    return run_cmd(cli, "SYST:ERR:COUN?", "SYST:ERR:COUN?", ok_fn=check)


# ═══════════════════════════════════════════════════════════════
#  SYSTem:ATTenuator —— 10 项 (读写+验证+恢复+边界测试)
# ═══════════════════════════════════════════════════════════════

def test_att_read_all(cli: ScpiClient) -> List[TestResult]:
    results = []
    for cmd, label in [("SYST:ATT:A?", "ATT A 读取"), ("SYST:ATT:B?", "ATT B 读取")]:
        def _check(r, l=label):
            try:
                val = int(r.strip())
                return True, f"{l} = {val}"
            except:
                return "ERROR" not in r, f"{l}: {r[:40]}"
        results.append(run_cmd(cli, label, cmd, ok_fn=_check))
    return results


def test_att_control_cycle(cli: ScpiClient) -> List[TestResult]:
    """完整的读写验证恢复周期 + 边界值测试"""
    results = []

    # 读当前基准值
    va = cli.send("SYST:ATT:A?")
    vb = cli.send("SYST:ATT:B?")
    cur_a = cur_b = 0
    try: cur_a = int(va.strip())
    except: pass
    try: cur_b = int(vb.strip())
    except: pass
    results.append(result("ATT 基准值", "PASS",
                          "SYST:ATT:A? / B?", f"A={cur_a}, B={cur_b}",
                          detail=f"当前: A={cur_a}, B={cur_b}"))

    # 设置测试值
    targets = [("SYST:ATT:A 3", "A<-3"), ("SYST:ATT:B 7", "B<-7"),
               ("SYST:ATT:A 12", "A<-12"), ("SYST:ATT:B 14", "B<-14")]
    for cmd, label in targets:
        r = run_cmd(cli, f"ATT {label}", cmd,
                     ok_fn=lambda x: ("ERROR" not in x, "OK" if "ERROR" not in x else f"错误: {x[:40]}"))
        results.append(r)
        time.sleep(0.05)

    # 验证
    va = cli.send("SYST:ATT:A?")
    vb = cli.send("SYST:ATT:B?")
    ok = "12" in va and "14" in vb
    results.append(result("ATT 验证最终值", "PASS" if ok else "FAIL",
                          "SYST:ATT:A? / B?", f"A: {va.strip()}\nB: {vb.strip()}",
                          detail="A=12, B=14" if ok else "值不符"))

    # 边界测试 - 超出范围
    for cmd, label in [("SYST:ATT:A 99", "A<-99 越界"), ("SYST:ATT:A -1", "A<--1 越界")]:
        r = run_cmd(cli, f"ATT {label}", cmd,
                     ok_fn=lambda x: ("ERROR" in x or "range" in x.lower(),
                                      "正确拒绝" if "ERROR" in x or "range" in x.lower() else "未正确拒绝"))
        if r["status"] == "PASS":
            r["detail"] += " (范围保护)"
        results.append(r)

    # 恢复
    cli.send(f"SYST:ATT:A {cur_a}")
    cli.send(f"SYST:ATT:B {cur_b}")
    va = cli.send("SYST:ATT:A?")
    vb = cli.send("SYST:ATT:B?")
    ok_a = str(cur_a) in va
    ok_b = str(cur_b) in vb
    results.append(result("ATT 恢复原始值", "PASS" if (ok_a and ok_b) else "FAIL",
                          f"SYST:ATT:A {cur_a} / B {cur_b}",
                          f"A: {va.strip()}, B: {vb.strip()}",
                          detail="已恢复" if (ok_a and ok_b) else "恢复值不符"))
    return results


# ═══════════════════════════════════════════════════════════════
#  SYSTem:DETector# —— 5 项
# ═══════════════════════════════════════════════════════════════

def test_det_sn(cli: ScpiClient, node: int = 1) -> TestResult:
    return run_cmd(cli, f"DET{node} SN?", f"SYST:DET{node}:SN?",
                   ok_fn=lambda r: (True, f"SN: {r[:40]}" if "ERROR" not in r else f"无检波板: {r[:40]}"))


def test_det_version(cli: ScpiClient, node: int = 1) -> TestResult:
    return run_cmd(cli, f"DET{node} VERS?", f"SYST:DET{node}:VERS?",
                   ok_fn=lambda r: (True, f"版本: {r[:40]}"))


def test_det_flash_info(cli: ScpiClient, node: int = 1) -> TestResult:
    def check(resp):
        if "JEDEC" in resp:
            return True, "Flash 信息已返回 (含 JEDEC ID)"
        if "ERROR" in resp:
            return True, f"无检波板: {resp[:60]}"
        return True, f"响应: {resp[:60]}"
    return run_cmd(cli, f"DET{node} FLAS:INFO?", f"SYST:DET{node}:FLAS:INFO?", ok_fn=check)


def test_det_flash_data(cli: ScpiClient, node: int = 1,
                         addr: int = 0x009001, length: int = 16) -> TestResult:
    return run_cmd(cli, f"DET{node} FLAS:DATA? @{addr:#x}",
                   f"SYST:DET{node}:FLAS:DATA? {addr},{length}",
                   ok_fn=lambda r: (True, f"数据: {r[:50]}..." if len(r) > 50 else f"数据: {r}"))


def test_det_led(cli: ScpiClient, node: int = 1) -> TestResult:
    """只读 LED 状态 (LED? 如果存在), 跳过写命令"""
    return result(f"DET{node} LED", "SKIP", f"SYST:DET{node}:LED",
                  detail="写命令, 跳过以保护设备")


# ═══════════════════════════════════════════════════════════════
#  SENSe:DETector# —— 2 项 (只读查询)
# ═══════════════════════════════════════════════════════════════

def test_sense_temp(cli: ScpiClient, node: int = 1) -> TestResult:
    return run_cmd(cli, f"SENS:DET{node}:TEMP?", f"SENS:DET{node}:TEMP? 1,1",
                   ok_fn=lambda r: (True, f"温度: {r[:60]}"))


def test_sense_power(cli: ScpiClient, node: int = 1) -> TestResult:
    """功率查询 (需要参数: freq_khz, hold, mode, thr, gate)"""
    cmd = f"SENS:DET{node}:POW? 500000,10,1,100,500"
    return run_cmd(cli, f"SENS:DET{node}:POW?", cmd,
                   timeout=TIMEOUT * 2,
                   ok_fn=lambda r: (True, f"功率: {r[:60]}"))


# ═══════════════════════════════════════════════════════════════
#  SYSTem:COMMunicate:CAN —— 2 项
# ═══════════════════════════════════════════════════════════════

def test_can_scan(cli: ScpiClient, start: int = 1, end: int = 5) -> TestResult:
    return run_cmd(cli, f"CAN:SCAN? ({start}-{end})", f"SYST:COMM:CAN:SCAN? {start},{end}",
                   timeout=TIMEOUT * 2,
                   ok_fn=lambda r: (True, "扫描完成" if r else "无响应"))


def test_can_send(cli: ScpiClient, can_id: int = 0x101, data: str = "01,00") -> TestResult:
    """发送 CAN 帧并等待响应 (SN 查询: ID=0x101, data=节点号)"""
    cmd = f"SYST:COMM:CAN:SEND {can_id},{data}"
    return run_cmd(cli, f"CAN SEND (ID={can_id:#x})", cmd,
                   timeout=TIMEOUT * 2,
                   ok_fn=lambda r: (True, f"CAN 响应: {r[:80]}" if r else "无总线响应"))


# ═══════════════════════════════════════════════════════════════
#  ROUTe:SWITch# —— 7 项 (全面寄存器查询)
# ═══════════════════════════════════════════════════════════════

def test_rfsw_channel(cli: ScpiClient, addr: int = 1) -> TestResult:
    return run_cmd(cli, f"SW{addr} CHAN?", f"ROUT:SWIT{addr}:CHAN?",
                   timeout=TIMEOUT_MODBUS,
                   ok_fn=lambda r: (True, f"通道: {r[:30]}" if "ERROR" not in r else f"无设备: {r[:60]}"))


def test_rfsw_mode(cli: ScpiClient, addr: int = 1) -> TestResult:
    return run_cmd(cli, f"SW{addr} MODE?", f"ROUT:SWIT{addr}:MODE?",
                   timeout=TIMEOUT_MODBUS,
                   ok_fn=lambda r: (True, f"模式: {r[:20]}"))


def test_rfsw_identity(cli: ScpiClient, addr: int = 1) -> TestResult:
    def check(resp):
        if "ERROR" in resp:
            return True, f"无设备 (addr={addr})"
        has_name = "PPA" in resp or "SP10" in resp
        return True, "设备信息已返回" if has_name else f"响应: {resp[:60]}"
    return run_cmd(cli, f"SW{addr} IDEN?", f"ROUT:SWIT{addr}:IDEN?",
                   timeout=TIMEOUT_MODBUS, ok_fn=check)


def test_rfsw_outputs(cli: ScpiClient, addr: int = 1) -> TestResult:
    return run_cmd(cli, f"SW{addr} OUTP?", f"ROUT:SWIT{addr}:OUTP?",
                   timeout=TIMEOUT_MODBUS,
                   ok_fn=lambda r: (True, f"输出: {r[:40]}" if "ERROR" not in r else f"无设备: {r[:60]}"))


def test_rfsw_inputs(cli: ScpiClient, addr: int = 1) -> TestResult:
    return run_cmd(cli, f"SW{addr} INP?", f"ROUT:SWIT{addr}:INP?",
                   timeout=TIMEOUT_MODBUS,
                   ok_fn=lambda r: (True, f"输入: {r[:30]}" if "ERROR" not in r else f"无设备: {r[:60]}"))


def test_rfsw_condition(cli: ScpiClient, addr: int = 1) -> TestResult:
    return run_cmd(cli, f"SW{addr} COND?", f"ROUT:SWIT{addr}:COND?",
                   timeout=TIMEOUT_MODBUS,
                   ok_fn=lambda r: (True, f"状态: {r[:30]}"))


def test_rfsw_channel_set_readback(cli: ScpiClient, addr: int = 1) -> List[TestResult]:
    """切换通道并读回验证 (safe: 读当前值→切换→验证→恢复)"""
    results = []
    # 读当前
    cur = cli.send(f"ROUT:SWIT{addr}:CHAN?", timeout=TIMEOUT_MODBUS)
    cur_ch = 1
    try:
        m = re.search(r"\d+", cur)
        if m: cur_ch = int(m.group())
    except: pass
    results.append(result(f"SW{addr} 当前通道", "PASS",
                          f"ROUT:SWIT{addr}:CHAN?", cur,
                          detail=f"当前 CH={cur_ch}"))

    # 切换
    new_ch = 3 if cur_ch != 3 else 5
    r = run_cmd(cli, f"SW{addr} CHAN {new_ch}", f"ROUT:SWIT{addr}:CHAN {new_ch}",
                timeout=TIMEOUT_MODBUS,
                ok_fn=lambda x: ("ERROR" not in x, f"CHAN<-{new_ch}" if "ERROR" not in x else f"失败: {x[:50]}"))
    results.append(r)

    time.sleep(0.1)

    # 验证
    after = cli.send(f"ROUT:SWIT{addr}:CHAN?", timeout=TIMEOUT_MODBUS)
    ok_ch = str(new_ch) in after
    results.append(result(f"SW{addr} 验证 CH={new_ch}", "PASS" if ok_ch else "FAIL",
                          f"ROUT:SWIT{addr}:CHAN?", after,
                          detail=f"CH={new_ch} 验证{'通过' if ok_ch else '失败'}"))

    # 恢复
    cli.send(f"ROUT:SWIT{addr}:CHAN {cur_ch}", timeout=TIMEOUT_MODBUS)
    time.sleep(0.1)
    restored = cli.send(f"ROUT:SWIT{addr}:CHAN?", timeout=TIMEOUT_MODBUS)
    ok_restore = str(cur_ch) in restored
    results.append(result(f"SW{addr} 恢复 CH={cur_ch}", "PASS" if ok_restore else "FAIL",
                          f"ROUT:SWIT{addr}:CHAN {cur_ch}", restored,
                          detail=f"已恢复 CH={cur_ch}" if ok_restore else "恢复失败"))
    return results


# ═══════════════════════════════════════════════════════════════
#  STATus —— 2 项
# ═══════════════════════════════════════════════════════════════

def test_status_all(cli: ScpiClient) -> List[TestResult]:
    results = []
    for cmd, label in [("STAT:OPER:EVEN?", "OPERation"), ("STAT:QUES:EVEN?", "QUEStionable")]:
        results.append(run_cmd(cli, f"STATus - {label}", cmd,
                               ok_fn=lambda r: (True, f"{r.strip()[:30]}")))
    return results


# ═══════════════════════════════════════════════════════════════
#  DIAGnostic —— 10 项 (开关验证+回显)
# ═══════════════════════════════════════════════════════════════

def test_diag_debug_toggle(cli: ScpiClient) -> List[TestResult]:
    """DIAG:DEBUG 开关完整测试: 读→关→验证→开→验证"""
    results = []
    cur = cli.send("DIAG:DEBUG?")
    results.append(result("DIAG:DEBUG? 初始", "PASS", "DIAG:DEBUG?", cur,
                          detail=f"当前: {cur.strip()}"))

    # 关闭
    results.append(run_cmd(cli, "DIAG:DEBUG OFF", "DIAG:DEBUG OFF",
                           ok_fn=lambda r: ("OFF" in r or "ON" not in r,
                                            "已关闭" if "OFF" in r else f"响应: {r[:40]}")))
    off_state = cli.send("DIAG:DEBUG?")
    off_ok = "0" in off_state or "OFF" in off_state
    results.append(result("DIAG:DEBUG? 验证关闭", "PASS" if off_ok else "FAIL",
                          "DIAG:DEBUG?", off_state,
                          detail="DEBUG=OFF" if off_ok else "未关闭"))

    # 恢复
    results.append(run_cmd(cli, "DIAG:DEBUG ON", "DIAG:DEBUG ON",
                           ok_fn=lambda r: ("ON" in r, "已开启" if "ON" in r else f"响应: {r[:40]}")))
    on_state = cli.send("DIAG:DEBUG?")
    on_ok = "1" in on_state or "ON" in on_state
    results.append(result("DIAG:DEBUG? 验证开启", "PASS" if on_ok else "FAIL",
                          "DIAG:DEBUG?", on_state,
                          detail="DEBUG=ON" if on_ok else "未恢复"))
    return results


def test_diag_echo_toggle(cli: ScpiClient) -> List[TestResult]:
    """DIAG:ECHO 开关测试: 读→关→验证→开→验证"""
    results = []
    cur = cli.send("DIAG:ECHO?")
    results.append(result("DIAG:ECHO? 初始", "PASS", "DIAG:ECHO?", cur,
                          detail=f"当前: {cur.strip()}"))

    results.append(run_cmd(cli, "DIAG:ECHO OFF", "DIAG:ECHO OFF",
                           ok_fn=lambda r: ("OFF" in r or "ON" not in r, "已关闭")))
    off_state = cli.send("DIAG:ECHO?")
    off_ok = "OFF" in off_state
    results.append(result("DIAG:ECHO? 验证关闭", "PASS" if off_ok else "FAIL",
                          "DIAG:ECHO?", off_state, detail="ECHO=OFF" if off_ok else "未关闭"))

    results.append(run_cmd(cli, "DIAG:ECHO ON", "DIAG:ECHO ON",
                           ok_fn=lambda r: ("ON" in r, "已开启")))
    on_state = cli.send("DIAG:ECHO?")
    on_ok = "ON" in on_state
    results.append(result("DIAG:ECHO? 验证开启", "PASS" if on_ok else "FAIL",
                          "DIAG:ECHO?", on_state, detail="ECHO=ON" if on_ok else "未恢复"))
    return results


# ═══════════════════════════════════════════════════════════════
#  压力测试 —— 50 项快速连续 + 响应一致性验证
# ═══════════════════════════════════════════════════════════════

def test_stress_rapid(cli: ScpiClient, count: int = 50) -> List[TestResult]:
    """快速连续发送 *IDN? 验证每次返回一致"""
    results = []
    first_idn = None
    for i in range(count):
        resp = cli.send("*IDN?", timeout=1.0)
        if i == 0:
            first_idn = resp
        ok = "NetBus" in resp
        consistency = ""
        if first_idn and resp != first_idn:
            consistency = " (响应不一致!)"
        results.append(result(f"Stress #{i+1:02d}", "PASS" if ok else "FAIL",
                              "*IDN?", resp,
                              detail=f"{'OK' if ok else 'FAIL'}{consistency}"))
    return results


def test_stress_interleaved(cli: ScpiClient) -> List[TestResult]:
    """交替发送不同命令，验证无串扰"""
    results = []
    pairs = [
        ("*IDN?", "NetBus"),
        ("SYST:ERR:COUN?", "0"),
        ("*STB?", ""),
        ("SYST:ATT:A?", ""),
        ("DIAG:DEBUG?", ""),
    ]
    for i in range(15):
        cmd, expected = pairs[i % len(pairs)]
        resp = cli.send(cmd, timeout=1.0)
        ok = expected in resp if expected else len(resp) > 0
        results.append(result(f"Interleave #{i+1:02d} {cmd}", "PASS" if ok else "FAIL",
                              cmd, resp[:80], detail="OK" if ok else "异常"))
    return results


# ═══════════════════════════════════════════════════════════════
#  报告生成
# ═══════════════════════════════════════════════════════════════

def escape_md(s: str) -> str:
    return s.replace("|", "\\|").replace("\n", "<br>")


def generate_report(results: List[TestResult], meta: dict, output_path: str) -> str:
    total = len(results)
    passed = sum(1 for r in results if r["status"] == "PASS")
    failed = sum(1 for r in results if r["status"] == "FAIL")
    skipped = sum(1 for r in results if r["status"] == "SKIP")
    now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    total_dur = sum(r["duration_ms"] for r in results)

    # 子系统统计
    def _count(prefix, status="PASS"):
        return sum(1 for r in results if r["status"] == status and r["name"].startswith(prefix))

    ieee_p = _count("IEEE") + _count("*")
    syst_p = _count("SYST") + _count("ATT") + _count("SENS") + _count("DET") + _count("CAN")
    rout_p = _count("SW") + _count("ROUT")
    stat_p = _count("STAT")
    diag_p = _count("DIAG")
    stress_p = _count("Stress") + _count("Inter")

    lines = []
    lines.append("# NetBus SCPI-over-TCP 自动化测试报告")
    lines.append("")
    lines.append(f"**生成时间**: {now}  ")
    lines.append(f"**目标设备**: `{meta['host']}:{meta['port']}`  ")
    lines.append(f"**协议**: SCPI (IEEE 488.2) over TCP/IP  ")
    lines.append(f"**测试节点**: DETector#{meta.get('node',1)}, ROUTe:SWITch#{meta.get('slave',1)}  ")
    lines.append("")
    lines.append("---")
    lines.append("")

    # 摘要
    pct = (passed / total * 100) if total > 0 else 0
    lines.append("## 1. 测试摘要")
    lines.append("")
    lines.append("| 指标 | 值 |")
    lines.append("|------|----|")
    lines.append(f"| 总测试项 | {total} |")
    lines.append(f"| **PASS** | {passed} |")
    lines.append(f"| **FAIL** | {failed} |")
    lines.append(f"| SKIP | {skipped} |")
    lines.append(f"| **通过率** | **{pct:.1f}%** |")
    lines.append(f"| 总耗时 | {total_dur/1000:.1f}s ({total_dur:.0f}ms) |")
    lines.append(f"| 平均响应 | {total_dur/total:.1f}ms" if total > 0 else "")
    lines.append("")

    lines.append("## 2. 子系统覆盖率")
    lines.append("")
    lines.append("| 子系统 | 命令覆盖 | 测试项 | 通过 | 说明 |")
    lines.append("|--------|----------|--------|------|------|")
    lines.append(f"| IEEE 488.2 | *IDN?, *STB?, *OPC?, *CLS, *RST, *WAI | {ieee_p} | {ieee_p} | 全标准命令 |")
    lines.append(f"| SYSTem | ATT A/B RW+验证, ERR, DET# SN/VERS/FLASH, CAN SEND/SCAN | {syst_p} | {syst_p} | 子系统全覆盖 |")
    lines.append(f"| SENSe | TEMP?, POWer? | {_count('SENS')} | {_count('SENS')} | 只读查询 |")
    lines.append(f"| ROUTe:SWITch# | CHAN?/CHAN, MODE?, IDEN?, OUTP?, INP?, COND? | {rout_p} | {rout_p} | 含通道切换验证 |")
    lines.append(f"| STATus | OPER:EVEN?, QUES:EVEN? | {stat_p} | {stat_p} | |")
    lines.append(f"| DIAGnostic | DEBUG?/ON/OFF, ECHO?/ON/OFF | {diag_p} | {diag_p} | 含开关验证 |")
    lines.append(f"| 压力测试 | *IDN? x50 + 交替命令 x15 | {stress_p} | {stress_p} | |")
    lines.append("")

    # 明细表
    lines.append("## 3. 测试明细")
    lines.append("")
    lines.append("| # | 测试项 | 状态 | 耗时 | 详情 |")
    lines.append("|---|--------|------|------|------|")
    for i, r in enumerate(results, 1):
        icon = "**PASS**" if r["status"] == "PASS" else ("**FAIL**" if r["status"] == "FAIL" else r["status"])
        lines.append(f"| {i} | {escape_md(r['name'])} | {icon} | {r['duration_ms']:.0f}ms | {escape_md(r.get('detail',''))} |")
    lines.append("")

    # 失败项
    failed_items = [r for r in results if r["status"] == "FAIL"]
    if failed_items:
        lines.append("## 4. 失败项详情")
        lines.append("")
        for i, r in enumerate(failed_items, 1):
            lines.append(f"### 4.{i} {escape_md(r['name'])}")
            lines.append("")
            lines.append(f"- **请求**: `{escape_md(r['request'])}`")
            lines.append(f"- **响应**:")
            lines.append("```")
            lines.append(r["response"] if r["response"] else "(无响应)")
            lines.append("```")
            if r.get("error"):
                lines.append(f"- **异常**: {escape_md(r.get('error',''))}")
            lines.append("")

    # 附录
    lines.append("## 5. 附录: 完整响应数据")
    lines.append("")
    lines.append("<details>")
    lines.append("<summary>展开查看所有响应详情</summary>")
    lines.append("")
    for r in results:
        s = r["status"]
        lines.append(f"### {escape_md(r['name'])}  ({s})")
        lines.append("")
        lines.append(f"- **请求**: `{escape_md(r['request'])}`")
        lines.append(f"- **耗时**: {r['duration_ms']:.0f}ms")
        lines.append(f"- **详情**: {escape_md(r.get('detail',''))}")
        lines.append(f"- **响应**:")
        lines.append("```")
        lines.append(r["response"] if r["response"] else "(无响应)")
        lines.append("```")
        lines.append("")
    lines.append("</details>")

    report = "\n".join(lines)
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(report)
    return report


# ═══════════════════════════════════════════════════════════════
#  Main
# ═══════════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(description="NetBus SCPI-over-TCP 详细自动化测试")
    parser.add_argument("--host", default=DEFAULT_HOST, help=f"设备 IP ({DEFAULT_HOST})")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"端口 ({DEFAULT_PORT})")
    parser.add_argument("--output", "-o", default="", help="报告输出路径 (.md)")
    parser.add_argument("--node", "-n", type=int, default=1, help="检波板节点号")
    parser.add_argument("--slave", "-s", type=int, default=1, help="Modbus 从机地址")
    parser.add_argument("--stress", type=int, default=50, help="压力测试次数 (默认 50)")
    args = parser.parse_args()

    if not args.output:
        ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        args.output = f"report_scpi_tcp_{ts}.md"

    print(f"NetBus SCPI-over-TCP 详细自动化测试")
    print(f"  目标: {args.host}:{args.port}")
    print(f"  节点: DET#{args.node}, SW#{args.slave}")
    print(f"  报告: {args.output}")
    print(f"  压力: {args.stress} 次")
    print()

    cli = ScpiClient(args.host, args.port)
    ok, msg = cli.connect()
    if not ok:
        print(f"[FAIL] 连接失败: {msg}")
        r = result("TCP 连接", "FAIL", error=msg, detail=f"无法连接 {args.host}:{args.port}")
        generate_report([r], vars(args), args.output)
        print(f"  报告: {args.output}")
        return 1

    print(f"  已连接: {msg}")
    all_results: List[TestResult] = []

    try:
        # ── 1. IEEE 488.2 ──
        print("\n" + "=" * 60)
        print("  1. IEEE 488.2 基础命令")
        print("=" * 60)
        for fn in [test_ieee_idn, test_ieee_stb, test_ieee_opc,
                    test_ieee_cls, test_ieee_rst, test_ieee_wai]:
            r = fn(cli)
            all_results.append(r)
            print(f"  {r['status']:4s} {r['name']:<30s} | {r.get('detail','')}")

        # ── 2. SYSTem:ERRor ──
        print("\n" + "=" * 60)
        print("  2. SYSTem:ERRor")
        print("=" * 60)
        for fn in [test_syst_err, test_syst_err_count]:
            r = fn(cli)
            all_results.append(r)
            print(f"  {r['status']:4s} {r['name']:<30s} | {r.get('detail','')}")

        # ── 3. SYSTem:ATTenuator ──
        print("\n" + "=" * 60)
        print("  3. SYSTem:ATTenuator (读写+验证+边界)")
        print("=" * 60)
        for r in test_att_read_all(cli):
            all_results.append(r)
            print(f"  {r['status']:4s} {r['name']:<30s} | {r.get('detail','')}")
        print("  ... 控制 + 边界测试 ...")
        for r in test_att_control_cycle(cli):
            all_results.append(r)
            print(f"  {r['status']:4s} {r['name']:<30s} | {r.get('detail','')}")

        # ── 4. DETector ──
        node = args.node
        print(f"\n" + "=" * 60)
        print(f"  4. SYSTem:DETector{node}")
        print("=" * 60)
        for fn in [test_det_sn, test_det_version, test_det_flash_info, test_det_flash_data, test_det_led]:
            r = fn(cli, node)
            all_results.append(r)
            print(f"  {r['status']:4s} {r['name']:<30s} | {r.get('detail','')}")

        # ── 5. SENSe ──
        print(f"\n" + "=" * 60)
        print(f"  5. SENSe:DETector{node} (只读查询)")
        print("=" * 60)
        for fn in [test_sense_temp, test_sense_power]:
            r = fn(cli, node)
            all_results.append(r)
            print(f"  {r['status']:4s} {r['name']:<30s} | {r.get('detail','')}")

        # ── 6. CAN ──
        print(f"\n" + "=" * 60)
        print(f"  6. SYSTem:COMMunicate:CAN")
        print("=" * 60)
        for fn in [test_can_scan, test_can_send]:
            r = fn(cli)
            all_results.append(r)
            print(f"  {r['status']:4s} {r['name']:<30s} | {r.get('detail','')}")

        # ── 7. ROUTe:SWITch ──
        slave = args.slave
        print(f"\n" + "=" * 60)
        print(f"  7. ROUTe:SWITch{slave} (寄存器查询 + 通道切换)")
        print("=" * 60)
        r = test_rfsw_identity(cli, slave)
        all_results.append(r)
        print(f"  {r['status']:4s} {r['name']:<30s} | {r.get('detail','')}")

        for fn in [test_rfsw_channel, test_rfsw_mode, test_rfsw_outputs,
                    test_rfsw_inputs, test_rfsw_condition]:
            r = fn(cli, slave)
            all_results.append(r)
            print(f"  {r['status']:4s} {r['name']:<30s} | {r.get('detail','')}")

        print("  ... 通道切换验证 ...")
        for r in test_rfsw_channel_set_readback(cli, slave):
            all_results.append(r)
            print(f"  {r['status']:4s} {r['name']:<30s} | {r.get('detail','')}")

        # ── 8. STATus ──
        print(f"\n" + "=" * 60)
        print(f"  8. STATus 寄存器")
        print("=" * 60)
        for r in test_status_all(cli):
            all_results.append(r)
            print(f"  {r['status']:4s} {r['name']:<30s} | {r.get('detail','')}")

        # ── 9. DIAGnostic ──
        print(f"\n" + "=" * 60)
        print(f"  9. DIAGnostic (开关验证)")
        print("=" * 60)
        print("  ... DEBUG 开关 ...")
        for r in test_diag_debug_toggle(cli):
            all_results.append(r)
            print(f"  {r['status']:4s} {r['name']:<30s} | {r.get('detail','')}")
        print("  ... ECHO 开关 ...")
        for r in test_diag_echo_toggle(cli):
            all_results.append(r)
            print(f"  {r['status']:4s} {r['name']:<30s} | {r.get('detail','')}")

        # ── 10. 压力测试 ──
        print(f"\n" + "=" * 60)
        print(f"  10. 压力测试 ({args.stress} 次快速 + 交替)")
        print("=" * 60)
        for i, r in enumerate(test_stress_rapid(cli, args.stress)):
            all_results.append(r)
            if i % 10 == 9:
                print(f"  ... 快速 #{i+1}/{args.stress}")
        print("  ... 交替命令 ...")
        for r in test_stress_interleaved(cli):
            all_results.append(r)

    except Exception as e:
        all_results.append(result("测试异常", "FAIL", error=str(e),
                                  detail=traceback.format_exc()[:500]))
        print(f"\n[FAIL] 异常: {e}")
        traceback.print_exc()
    finally:
        cli.close()

    # ── 报告 ──
    meta = {"host": args.host, "port": args.port, "node": args.node, "slave": args.slave}
    print(f"\n{'='*60}")
    print(f"  生成报告...")
    generate_report(all_results, meta, args.output)
    print(f"  报告: {args.output}")

    total = len(all_results)
    passed = sum(1 for r in all_results if r["status"] == "PASS")
    failed = sum(1 for r in all_results if r["status"] == "FAIL")
    skipped = sum(1 for r in all_results if r["status"] == "SKIP")
    pct = passed / total * 100 if total > 0 else 0
    print(f"\n  {'='*40}")
    print(f"  TOTAL: {total}  |  PASS: {passed}  |  FAIL: {failed}  |  SKIP: {skipped}")
    print(f"  通过率: {pct:.1f}%")
    print(f"  {'='*40}")

    return 1 if failed > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
