Param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet("build", "upload", "monitor")]
    [string]$Action,
    
    [Parameter(Mandatory = $false, Position = 1, ValueFromRemainingArguments = $true)]
    [string[]]$OptParams
)

$FW_VERSION = "v1.4.1-6bit-color"
# Arduino CLI executable name and config
$ARDUINO_CLI = "arduino-cli.exe"
$CONFIG_DIR = "$($env:USERPROFILE)\.arduinoIDE"
# Default port to upload to
$PORT = "COM5"
$BAUDRATE = "9600"
$PORT_CONFIG = "--config baudrate=$BAUDRATE" -split "\s+"
# Optional verbose compile/upload trigger
$VERBOSE = "--verbose"
# Exter build flags
$ExtraBuildFlags = "build.flags.optimize=-O3" -split "\s+"
$ExtraBuildFlags += "build.extra_flags=""-DFW_VERSION=\""$FW_VERSION\""""" -split "\s+"
# $ExtraBuildFlags += "compiler.c.extra_flags=-save-temps compiler.cpp.extra_flags=-save-temps" -split "\s+"
# Common parameters
$BaseParams = "--config-dir $CONFIG_DIR --port $PORT" -split "\s+"

$BuildFlags = @()

foreach ($buildFlag in $ExtraBuildFlags) {
    $BuildFlags += "--build-property" 
    $BuildFlags += "$buildFlag"
}

$Params = @()

if ($Action -eq "build") {
    $Params = ("compile $VERBOSE" -split "\s+") + $BuildFlags
}

if ($Action -eq "upload") {
    $Params = "upload $VERBOSE" -split "\s+"
}

if ($Action -eq "monitor") {
    $Params = "monitor $PORT_CONFIG" -split "\s+"
}

iF ($Params.Count -gt 0) {
    & $ARDUINO_CLI ($Params + $BaseParams + $OptParams)
}