@echo off
setlocal

echo Building FalloutChat (xmake)...
xmake -y
if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b %ERRORLEVEL%
)

echo.
echo Build and Deployment Successful!
endlocal
