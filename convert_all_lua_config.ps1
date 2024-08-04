$modelsPath = "models"
$folders = Get-ChildItem -Path $modelsPath -Recurse -Filter "model.lua" | Select-Object -ExpandProperty DirectoryName
foreach ($folder in $folders) {
    $modelLuaPath = Join-Path -Path $folder -ChildPath "model.lua"
    $configBinPath = Join-Path -Path $folder -ChildPath "config.bin"
    $command = "./lua_config_convert.exe $modelLuaPath $configBinPath"
    Invoke-Expression $command
}
