# Save the original directory to return to it later
$OriginalDirectory = Get-Location

# Create build directory if it doesn't exist
if (-not (Test-Path -Path "$PSScriptRoot/build")) {
    New-Item -ItemType Directory -Path "$PSScriptRoot/build" | Out-Null
}

# cd into the build directory, clean, and run CMake to configure and build (including tests and docs)
Set-Location $PSScriptRoot/build
Remove-Item * -Recurse -Force -ErrorAction SilentlyContinue
cmake .. -DBUILD_TESTS=ON -DBUILD_DOCS=ON -DARTIE_CAN_LOGGING_ENABLED=1
cmake --build . --config Debug

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed. Exiting."
    exit $LASTEXITCODE
}

if (Test-Path -Path "$PSScriptRoot/build/docs/html/index.html") {
    Write-Host "API documentation generated at $PSScriptRoot/build/docs/html/index.html"
}

# Run tests with verbose output
ctest -C Debug -V

# Change back to the original directory
Set-Location $OriginalDirectory
