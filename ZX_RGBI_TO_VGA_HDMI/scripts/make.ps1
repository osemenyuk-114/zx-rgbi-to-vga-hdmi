Param(
    [Parameter(Mandatory = $True, Position = 0)]
    [ValidateSet("build", "upload", "monitor")]
    [string]$Action
)

# Arduino CLI executable name and config
$ARDUINO_CLI = "arduino-cli.exe"
$CONFIG_DIR = "$($env:USERPROFILE)\.arduinoIDE"
# Arduino CLI Board type
$BOARD_TYPE = "rp2040:rp2040:rpipico"
# Default port to upload to
$SERIAL_PORT = "COM6"

# Build flags
$EXTRA_BUILD_FLAGS = "-O3"

# Optional verbose compile/upload trigger
$VERBOSE = "-v"

$buildFlags = $EXTRA_BUILD_FLAGS.Split(" ")

$BUILD_FLAGS = "" # --build-property compiler.c.extra_flags=-save-temps --build-property compiler.cpp.extra_flags=-save-temps"

foreach ($buildFlag in $buildFlags) {
    if ($buildFlag -match "-O\d{1}") {
        $BUILD_FLAGS += " --build-property build.flags.optimize=$buildFlag"
    }
}

if ($Action -eq "build") {
    $Params = $("compile --config-dir $CONFIG_DIR $VERBOSE $BUILD_FLAGS -b $BOARD_TYPE") -split "\s+"
}

if ($Action -eq "upload") {
    $Params = $("upload --config-dir $CONFIG_DIR $VERBOSE -p $SERIAL_PORT --fqbn $BOARD_TYPE") -split "\s+"
}

if ($Action -eq "monitor") {
    $Params = $("monitor --config-dir $CONFIG_DIR -p $SERIAL_PORT") -split "\s+"
}

iF ($Params.Count -gt 0)
{
    & $ARDUINO_CLI $Params
}