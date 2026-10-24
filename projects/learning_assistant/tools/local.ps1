param([ValidateSet('build','sim','test')][string]$Action = 'build')
$ErrorActionPreference = 'Stop'
$projectPath = Split-Path $PSScriptRoot -Parent
$workspacePath = (Resolve-Path (Join-Path $projectPath '../..')).Path
$sdkPath = if ($env:IDF_PATH) { $env:IDF_PATH } else { Join-Path $workspacePath '.codex-runs/esp-idf-mosaico' }
$toolPath = if ($env:IDF_TOOLS_PATH) { $env:IDF_TOOLS_PATH } else { Join-Path $workspacePath '.codex-runs/idf-tools' }
$revision = git -C $sdkPath rev-parse HEAD
if ($revision -ne '7b9cc1ac79f865983f59bb8ff3ff43eb74ff1dbe') { throw 'Pinned ESP-IDF checkout required' }
$env:IDF_TOOLS_PATH = $toolPath
$env:PYTHONUTF8 = '1'
$env:Path = (Join-Path $toolPath 'tools/cmake/4.0.3/bin') + ';' + (Join-Path $toolPath 'tools/ninja/1.12.1') + ';' + $env:Path
if ($Action -eq 'build') {
    python (Join-Path $workspacePath 'submodule/esp-mosaico-utils/mosaico-tools/skills/idf-low-noise-build/scripts/idf_low_noise_build.py') --project $projectPath --idf-path $sdkPath build
} elseif ($Action -eq 'sim') {
    $zigCompiler = Join-Path $env:APPDATA 'Python/Python313/Scripts/python-zig.exe'
    if (Test-Path -LiteralPath $zigCompiler) { $env:CC = '"' + $zigCompiler + '" cc'; $env:CMAKE_GENERATOR = 'Ninja' }
    $env:GSP_SIM_DYNAMIC_FONT = Join-Path $projectPath 'ui/fonts/LearningSans.ttf'
    python (Join-Path $workspacePath 'mosaico.py') project sim --project $projectPath --interactive
} else {
    & (Join-Path $toolPath 'tools/cmake/4.0.3/bin/cmake.exe') --build (Join-Path $projectPath 'build-sim') --target learning_model_test provider_protocol_test orientation_logic_test viewport_test speech_conversation_test controls_test questions_test learning_backend
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & (Join-Path $projectPath 'build-sim/learning_model_test.exe')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & (Join-Path $projectPath 'build-sim/provider_protocol_test.exe')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & (Join-Path $projectPath 'build-sim/orientation_logic_test.exe')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & (Join-Path $projectPath 'build-sim/viewport_test.exe')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & (Join-Path $projectPath 'build-sim/speech_conversation_test.exe')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & (Join-Path $projectPath 'build-sim/controls_test.exe')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & (Join-Path $projectPath 'build-sim/questions_test.exe')
}
exit $LASTEXITCODE
