Param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet("build", "upload", "monitor")]
    [string]$Action,
    
    [Parameter(Mandatory = $false, Position = 1, ValueFromRemainingArguments = $true)]
    [System.Collections.Generic.List[string]]$OptParams,

    [Parameter(Mandatory = $false)]
    [switch]$AllWarnings
)

# Arduino CLI executable name and config
$ARDUINO_CLI = "arduino-cli.exe"
$CONFIG_DIR = "$($env:USERPROFILE)\.arduinoIDE"
$SKETCH_YAML = ".\sketch.yaml"

# Get the FQBN from sketch.yaml
if (Test-Path -Path $SKETCH_YAML -PathType Leaf) {
    $FQBN = $(Get-Content -Path $SKETCH_YAML | Where-Object { $_ -match "default_fqbn" }).Split(" ")[1]
}
# If FQBN is found, get the port for the detected board
if ($FQBN -ne "") {
    $PORT = $(& $ARDUINO_CLI ("board list --json" -split "\s+") | ConvertFrom-Json).detected_ports | ForEach-Object { if ($_.matching_boards.fqbn -contains $FQBN) { $_.port.address } }
}
$PORT = "COM5"
$BAUDRATE = "9600"
$PORT_CONFIG = "--config baudrate=$BAUDRATE" -split "\s+"
# Optional verbose compile/upload trigger
$VERBOSE = "--verbose"
# Extra build flags
$ExtraBuildFlags = "build.flags.optimize=-O3" -split "\s+"

# Add focused warning flags when -AllWarnings is specified
if ($AllWarnings) {
    # Focused warnings for user code - suppress noisy system header warnings
    $WarningFlags = "-Wall -Wunused-variable -Wunused-function -Wunused-parameter -Wshadow -Wuninitialized -Wmissing-declarations -Wno-redundant-decls -Wno-undef -Wno-sign-conversion -Wno-conversion -Wno-float-conversion"
    
    $ExtraBuildFlags += "compiler.c.extra_flags=$WarningFlags"
    $ExtraBuildFlags += "compiler.cpp.extra_flags=$WarningFlags"
}

# $ExtraBuildFlags += "compiler.c.extra_flags=-save-temps compiler.cpp.extra_flags=-save-temps" -split "\s+"

# Common parameters
$BaseParams = "--config-dir $CONFIG_DIR" -split "\s+"

$BuildFlags = @()

foreach ($buildFlag in $ExtraBuildFlags) {
    $BuildFlags += "--build-property" 
    $BuildFlags += "$buildFlag"
}

$Params = @()

if ($Action -eq "build") {
    if ($OptParams -contains "--upload") {
        if ($null -ne $PORT) {
            $UPLOAD_PORT = "--upload --port $PORT" -split "\s+"
        }
        else {
            $UPLOAD_PORT = ""
            $OptParams.Remove("--upload")
        }
    }

    $Params = ("compile $UPLOAD_PORT $VERBOSE" -split "\s+") + $BuildFlags
}

if (($Action -eq "upload") -and ($null -ne $PORT)) {
    $Params = "upload --port $PORT $VERBOSE" -split "\s+"
}

if (($Action -eq "monitor") -and ($null -ne $PORT)) {
    $Params = "monitor --port $PORT $PORT_CONFIG" -split "\s+"
}

iF ($Params.Count -gt 0) {
    & $ARDUINO_CLI ($Params + $BaseParams + $OptParams)
}