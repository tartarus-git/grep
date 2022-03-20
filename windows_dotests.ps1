$previousWorkingDir = pwd
cd $(Join-Path -Path $PSScriptRoot -ChildPath "test")
./test.exe windows_test_script.script
cd $previousWorkingDir
