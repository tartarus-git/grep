$previousWorkingDir = pwd
cd $(Join-Path -Path $PSScriptRoot -ChildPath "test")
./windows_test.ps1
cd $previousWorkingDir
