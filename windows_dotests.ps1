$previousWorkingDir = pwd
cd $(Join-Path -Path $PSScriptRoot -ChildPath "test")
./test.ps1
cd $previousWorkingDir