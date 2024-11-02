function Monitor-File {
    param (
        [string]$FilePath
    )

    # Initial content of the file
    $initialContent = Get-Content $FilePath

    while ($true) {
        # Current content of the file
        $currentContent = Get-Content $FilePath

        # Check for changes in content
        if ($initialContent -ne $currentContent) {
            Clear-Host
            Write-Host "File:" $FilePath "`n" # Display the file path
            Get-Content $FilePath  # Display the new content
            $initialContent = $currentContent
        }

        # Wait for a short duration before checking again
        Start-Sleep -Seconds 1
    }
}

# Check if the file path is provided as an argument
if (-not $args) {
    Write-Host "Usage:" $MyInvocation.MyCommand.Name "<file-path>"
    exit 1
}

# Start monitoring the file
Monitor-File -FilePath $args[0]
