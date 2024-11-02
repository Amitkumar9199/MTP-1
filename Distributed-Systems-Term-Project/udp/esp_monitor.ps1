function Monitor-ESP {
    param (
        [string]$esp_ip,
        [string]$mode,
        [string]$save_file_name
    )

    # Check if the mode is valid
    if ($mode -ne "info" -and $mode -ne "comms") {
        Write-Host "Invalid mode"
        exit 1
    }

    # Check if the ESP IP is reachable
    while (-not (Test-Connection -ComputerName $esp_ip -Count 1 -Quiet)) {
        Write-Host "ESP IP is not reachable"
        Start-Sleep -Seconds 1
    }

    # Start monitoring the ESP
    $url = "http://$esp_ip/$mode"
    while ($true) {
        $response = Invoke-WebRequest -Uri $url -Method Get
        Clear-Host
        if ($mode -eq "info") {
            Write-Host "ESP Information:" $url "`n"
            Write-Host $($response.Content | ConvertFrom-Json | ConvertTo-Json)
            # Save the response to a file
            $response.Content | ConvertFrom-Json | ConvertTo-Json | Out-File $save_file_name
        } else {
            Write-Host "ESP Communications:" $url "`n"
            Write-Host $response.Content
            # Save the response to a file
            $response.Content | Out-File $save_file_name
        }

        Start-Sleep -Seconds 1
    }
}


# Check if the file path is provided as an argument
if (-not $args -or $args.Length -lt 3) {
    Write-Host "Usage:" $MyInvocation.MyCommand.Name "<esp-ip>" "<info/comms>" "<save_file_name>"
    exit 1
}

# Start monitoring the file
Monitor-ESP $args[0] $args[1] $args[2]
