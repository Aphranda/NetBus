@echo off
REM ============================================================
REM  NetBus SCPI-over-TCP 自动化测试 (Conda 环境)
REM  用法: run_test.bat [设备IP, 默认 192.168.1.10]
REM ============================================================
setlocal enabledelayedexpansion

set DEVICE_IP=%1
if "%DEVICE_IP%"=="" set DEVICE_IP=192.168.1.10

set SCRIPT_DIR=%~dp0
set REPORT_DIR=%SCRIPT_DIR%reports
if not exist "%REPORT_DIR%" mkdir "%REPORT_DIR%"

echo.
echo ============================================================
echo   NetBus SCPI-over-TCP 自动化测试
echo   目标: %DEVICE_IP%:5025
echo ============================================================
echo.

REM --- 检查 conda 是否可用 ---
where conda >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [FAIL] 未找到 conda，请安装 Miniconda 或 Anaconda
    echo         https://docs.conda.io/en/latest/miniconda.html
    exit /b 1
)

REM --- 创建/更新 conda 环境 ---
set ENV_NAME=netbus-test
call conda env list | findstr /C:"%ENV_NAME%" >nul
if %ERRORLEVEL% NEQ 0 (
    echo [INFO] 创建 conda 环境: %ENV_NAME%
    call conda env create -f "%SCRIPT_DIR%environment.yml" -q
    if %ERRORLEVEL% NEQ 0 (
        echo [FAIL] 环境创建失败
        exit /b 1
    )
)

REM --- 激活环境并运行测试 ---
set TIMESTAMP=%date:~0,4%%date:~5,2%%date:~8,2%_%time:~0,2%%time:~3,2%%time:~6,2%
set TIMESTAMP=%TIMESTAMP: =0%
set REPORT=%REPORT_DIR%\report_%TIMESTAMP%.md

echo [INFO] 运行测试 ...
call conda run -n %ENV_NAME% python "%SCRIPT_DIR%scpi_test.py" --host %DEVICE_IP% --output "%REPORT%"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ============================================================
    echo   测试完成 - 全部通过
    echo   报告: %REPORT%
    echo ============================================================
) else (
    echo.
    echo ============================================================
    echo   测试完成 - 有失败项，请查看报告
    echo   报告: %REPORT%
    echo ============================================================
)

endlocal
