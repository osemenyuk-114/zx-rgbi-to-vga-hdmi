Param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet("build", "upload", "monitor")]
    [string]$Action,
    
    [Parameter(Mandatory = $false, Position = 1, ValueFromRemainingArguments = $true)]
    [string[]]$OptParams
)

# Arduino CLI executable name and config
$ARDUINO_CLI = "arduino-cli.exe"
$CONFIG_DIR = "$($env:USERPROFILE)\.arduinoIDE"
# Arduino CLI Board type
$FQBN = "rp2040:rp2040:rpipico"
# Default port to upload to
$PORT = "COM6"
# Optional verbose compile/upload trigger
$VERBOSE = "--verbose"
# Exter build flags
$ExtraBuildFlags = "build.flags.optimize=-O3" -split "\s+"
# $ExtraBuildFlags += "compiler.c.extra_flags=-save-temps compiler.cpp.extra_flags=-save-temps" -split "\s+"

$BuildFlags = @()

foreach ($buildFlag in $ExtraBuildFlags) {
    $BuildFlags += "--build-property" 
    $BuildFlags += "$buildFlag"
}

$Params = @()

if ($Action -eq "build") {
    $Params = ("compile --config-dir $CONFIG_DIR $VERBOSE --port $PORT --fqbn $FQBN" -split "\s+") + $BuildFlags
}

if ($Action -eq "upload") {
    $Params = "upload --config-dir $CONFIG_DIR $VERBOSE --port $PORT --fqbn $FQBN" -split "\s+"
}

if ($Action -eq "monitor") {
    $Params = "monitor --config-dir $CONFIG_DIR --port $PORT" -split "\s+"
}

iF ($Params.Count -gt 0) {
    & $ARDUINO_CLI ($Params + $OptParams)
}