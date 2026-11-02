@echo off
chcp 65001 >nul
title 抓 COM8 + 画波形

set BIN=D:\PMSMfoc\debug\com8_capture.bin
set PNG=D:\PMSMfoc\debug\vofa_waveform.png
set PY=C:\Users\m'm'm\.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\python\python.exe

echo.
echo [1/2] 抓取 COM8 数据 5 秒（200Hz x 5s = 约 1000 帧）...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
 "$p=New-Object System.IO.Ports.SerialPort('COM8',115200,'None',8,'One');" ^
 "$p.ReadTimeout=300;" ^
 "$m=New-Object System.IO.MemoryStream;" ^
 "try{$p.Open()}catch{Write-Host ('打开失败: '+$_.Exception.Message); exit 1};" ^
 "$sw=[Diagnostics.Stopwatch]::StartNew();" ^
 "$b=New-Object byte[] 4096;" ^
 "while($sw.ElapsedMilliseconds -lt 5000){try{$n=$p.Read($b,0,4096); if($n -gt 0){$m.Write($b,0,$n)}}catch{}};" ^
 "$p.Close();" ^
 "[IO.File]::WriteAllBytes('%BIN%',$m.ToArray());" ^
 "Write-Host ('抓到 ' + $m.Length + ' 字节, 约 ' + [int]($m.Length/32) + ' 帧')"

if %errorlevel% neq 0 goto end

echo.
echo [2/2] 解码并画图...
if not exist "%PY%" (
  echo 找不到 python: %PY%
  goto end
)
"%PY%" "D:\PMSMfoc\debug\analyze.py"

echo.
echo 完成，图在 %PNG%
start "" "%PNG%"

:end
echo.
pause
