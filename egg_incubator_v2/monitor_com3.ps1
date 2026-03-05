try {
    $port = New-Object System.IO.Ports.SerialPort('COM3', 115200, 'None', 8, 'One')
    $port.NewLine = "`n"
    $port.ReadTimeout = 500
    $port.Open()
    Write-Output "Connected to COM3 at 115200"
    while ($true) {
        try {
            $line = $port.ReadLine()
            if ($line -ne $null) { Write-Output $line }
        } catch {
            Start-Sleep -Milliseconds 50
        }
    }
} catch {
    Write-Error $_.Exception.Message
}
