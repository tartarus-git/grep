@echo off
set testParams=""
set /A index=0
goto :skipFirst
:test
type .\inputs\source.cpp | ..\Debug\grep.exe %testParams% > output.txt
git diff --no-index ".\templates\%index%.txt" "output.txt"
set /A index=%index%+1
exit /b
:skipFirst

set testParams="a" call :test
echo %index%
set setParams="-a a" call :test
echo %index%
set setParams="-c a" call :test
set setParams="-ca a" call :test
set setParams="-l a" call :test
set setParams="-al a" call :test
set setParams="-cal a" call :test
set setParams="-cl a" call :test

set setParams="-v a" call :test
set setParams="-v -a a" call :test
set setParams="-v -c a" call :test
set setParams="-v -ca a" call :test
set setParams="-v -l a" call :test
set setParams="-v -al a" call :test
set setParams="-v -cal a" call :test
set setParams="-v -cl a" call :test

set setParams="--context 5 a" call :test
set setParams="--context 5 -a a" call :test
set setParams="--context 5 -c a" call :test
set setParams="--context 5 -ca a" call :test
set setParams="--context 1 -l a" call :test
set setParams="--context 1 -al a" call :test
set setParams="--context 1 -cal a" call :test
set setParams="--context 1 -cl A" call :test

set setParams="--context 5 -v a" call :test
set setParams="--context 5 -v -a a" call :test
set setParams="--context 1 -v -c a" call :test
set setParams="--context 1 -v -ca a" call :test
set setParams="--context 1 -v -l a" call :test
set setParams="--context 1 -v -al a" call :test
set setParams="--context 1 -v -cal a" call :test
set setParams="--context 1 -v -cl a" call :test

set setParams="--only-line-nums a" call :test
set setParams="--only-line-nums -a a" call :test
set setParams="--only-line-nums -c a" call :test
set setParams="--only-line-nums -ca a" call :test
set setParams="--only-line-nums -l a" call :test
set setParams="--only-line-nums -al a" call :test
set setParams="--only-line-nums -cal a" call :test
set setParams="--only-line-nums -cl a" call :test

set setParams="--only-line-nums -v a" call :test
set setParams="--only-line-nums -v -a a" call :test
set setParams="--only-line-nums -v -c a" call :test
set setParams="--only-line-nums -v -ca a" call :test
set setParams="--only-line-nums -v -l a" call :test
set setParams="--only-line-nums -v -al a" call :test
set setParams="--only-line-nums -v -cal a" call :test
set setParams="--only-line-nums -v -cl a" call :test

set setParams="--only-line-nums --context 5 a" call :test
set setParams="--only-line-nums --context 5 -a a" call :test
set setParams="--only-line-nums --context 5 -c a" call :test
set setParams="--only-line-nums --context 5 -ca a" call :test
set setParams="--only-line-nums --context 1 -l a" call :test
set setParams="--only-line-nums --context 1 -al a" call :test
set setParams="--only-line-nums --context 1 -cal a" call :test
set setParams="--only-line-nums --context 1 -cl A" call :test

set setParams="--only-line-nums --context 5 -v a" call :test
set setParams="--only-line-nums --context 5 -v -a a" call :test
set setParams="--only-line-nums --context 1 -v -c a" call :test
set setParams="--only-line-nums --context 1 -v -ca a" call :test
set setParams="--only-line-nums --context 1 -v -l a" call :test
set setParams="--only-line-nums --context 1 -v -al a" call :test
set setParams="--only-line-nums --context 1 -v -cal a" call :test
set setParams="--only-line-nums --context 1 -v -cl a" call :test

set setParams="--color on a" call :test
set setParams="--color on -a a" call :test
set setParams="--color on -c a" call :test
set setParams="--color on -ca a" call :test
set setParams="--color on -l a" call :test
set setParams="--color on -al a" call :test
set setParams="--color on -cal a" call :test
set setParams="--color on -cl a" call :test

set setParams="--color on -v a" call :test
set setParams="--color on -v -a a" call :test
set setParams="--color on -v -c a" call :test
set setParams="--color on -v -ca a" call :test
set setParams="--color on -v -l a" call :test
set setParams="--color on -v -al a" call :test
set setParams="--color on -v -cal a" call :test
set setParams="--color on -v -cl a" call :test

set setParams="--color on --context 5 a" call :test
set setParams="--color on --context 5 -a a" call :test
set setParams="--color on --context 5 -c a" call :test
set setParams="--color on --context 5 -ca a" call :test
set setParams="--color on --context 1 -l a" call :test
set setParams="--color on --context 1 -al a" call :test
set setParams="--color on --context 1 -cal a" call :test
set setParams="--color on --context 1 -cl A" call :test

set setParams="--color on --context 5 -v a" call :test
set setParams="--color on --context 5 -v -a a" call :test
set setParams="--color on --context 1 -v -c a" call :test
set setParams="--color on --context 1 -v -ca a" call :test
set setParams="--color on --context 1 -v -l a" call :test
set setParams="--color on --context 1 -v -al a" call :test
set setParams="--color on --context 1 -v -cal a" call :test
set setParams="--color on --context 1 -v -cl a" call :test

set setParams="--color on --only-line-nums a" call :test
set setParams="--color on --only-line-nums -a a" call :test
set setParams="--color on --only-line-nums -c a" call :test
set setParams="--color on --only-line-nums -ca a" call :test
set setParams="--color on --only-line-nums -l a" call :test
set setParams="--color on --only-line-nums -al a" call :test
set setParams="--color on --only-line-nums -cal a" call :test
set setParams="--color on --only-line-nums -cl a" call :test

set setParams="--color on --only-line-nums -v a" call :test
set setParams="--color on --only-line-nums -v -a a" call :test
set setParams="--color on --only-line-nums -v -c a" call :test
set setParams="--color on --only-line-nums -v -ca a" call :test
set setParams="--color on --only-line-nums -v -l a" call :test
set setParams="--color on --only-line-nums -v -al a" call :test
set setParams="--color on --only-line-nums -v -cal a" call :test
set setParams="--color on --only-line-nums -v -cl a" call :test

set setParams="--color on --only-line-nums --context 5 a" call :test
set setParams="--color on --only-line-nums --context 5 -a a" call :test
set setParams="--color on --only-line-nums --context 5 -c a" call :test
set setParams="--color on --only-line-nums --context 5 -ca a" call :test
set setParams="--color on --only-line-nums --context 1 -l a" call :test
set setParams="--color on --only-line-nums --context 1 -al a" call :test
set setParams="--color on --only-line-nums --context 1 -cal a" call :test
set setParams="--color on --only-line-nums --context 1 -cl A" call :test

set setParams="--color on --only-line-nums --context 5 -v a" call :test
set setParams="--color on --only-line-nums --context 5 -v -a a" call :test
set setParams="--color on --only-line-nums --context 1 -v -c a" call :test
set setParams="--color on --only-line-nums --context 1 -v -ca a" call :test
set setParams="--color on --only-line-nums --context 1 -v -l a" call :test
set setParams="--color on --only-line-nums --context 1 -v -al a" call :test
set setParams="--color on --only-line-nums --context 1 -v -cal a" call :test
set setParams="--color on --only-line-nums --context 1 -v -cl a" call :test

set setParams="--color off a" call :test
set setParams="--color off -a a" call :test
set setParams="--color off -c a" call :test
set setParams="--color off -ca a" call :test
set setParams="--color off -l a" call :test
set setParams="--color off -al a" call :test
set setParams="--color off -cal a" call :test
set setParams="--color off -cl a" call :test

set setParams="--color off -v a" call :test
set setParams="--color off -v -a a" call :test
set setParams="--color off -v -c a" call :test
set setParams="--color off -v -ca a" call :test
set setParams="--color off -v -l a" call :test
set setParams="--color off -v -al a" call :test
set setParams="--color off -v -cal a" call :test
set setParams="--color off -v -cl a" call :test

set setParams="--color off --context 5 a" call :test
set setParams="--color off --context 5 -a a" call :test
set setParams="--color off --context 5 -c a" call :test
set setParams="--color off --context 5 -ca a" call :test
set setParams="--color off --context 1 -l a" call :test
set setParams="--color off --context 1 -al a" call :test
set setParams="--color off --context 1 -cal a" call :test
set setParams="--color off --context 1 -cl A" call :test

set setParams="--color off --context 5 -v a" call :test
set setParams="--color off --context 5 -v -a a" call :test
set setParams="--color off --context 1 -v -c a" call :test
set setParams="--color off --context 1 -v -ca a" call :test
set setParams="--color off --context 1 -v -l a" call :test
set setParams="--color off --context 1 -v -al a" call :test
set setParams="--color off --context 1 -v -cal a" call :test
set setParams="--color off --context 1 -v -cl a" call :test

set setParams="--color off --only-line-nums a" call :test
set setParams="--color off --only-line-nums -a a" call :test
set setParams="--color off --only-line-nums -c a" call :test
set setParams="--color off --only-line-nums -ca a" call :test
set setParams="--color off --only-line-nums -l a" call :test
set setParams="--color off --only-line-nums -al a" call :test
set setParams="--color off --only-line-nums -cal a" call :test
set setParams="--color off --only-line-nums -cl a" call :test

set setParams="--color off --only-line-nums -v a" call :test
set setParams="--color off --only-line-nums -v -a a" call :test
set setParams="--color off --only-line-nums -v -c a" call :test
set setParams="--color off --only-line-nums -v -ca a" call :test
set setParams="--color off --only-line-nums -v -l a" call :test
set setParams="--color off --only-line-nums -v -al a" call :test
set setParams="--color off --only-line-nums -v -cal a" call :test
set setParams="--color off --only-line-nums -v -cl a" call :test

set setParams="--color off --only-line-nums --context 5 a" call :test
set setParams="--color off --only-line-nums --context 5 -a a" call :test
set setParams="--color off --only-line-nums --context 5 -c a" call :test
set setParams="--color off --only-line-nums --context 5 -ca a" call :test
set setParams="--color off --only-line-nums --context 1 -l a" call :test
set setParams="--color off --only-line-nums --context 1 -al a" call :test
set setParams="--color off --only-line-nums --context 1 -cal a" call :test
set setParams="--color off --only-line-nums --context 1 -cl A" call :test

set setParams="--color off --only-line-nums --context 5 -v a" call :test
set setParams="--color off --only-line-nums --context 5 -v -a a" call :test
set setParams="--color off --only-line-nums --context 1 -v -c a" call :test
set setParams="--color off --only-line-nums --context 1 -v -ca a" call :test
set setParams="--color off --only-line-nums --context 1 -v -l a" call :test
set setParams="--color off --only-line-nums --context 1 -v -al a" call :test
set setParams="--color off --only-line-nums --context 1 -v -cal a" call :test
set setParams="--color off --only-line-nums --context 1 -v -cl a" call :test

set setParams="--color auto a" call :test
set setParams="--color auto -a a" call :test
set setParams="--color auto -c a" call :test
set setParams="--color auto -ca a" call :test
set setParams="--color auto -l a" call :test
set setParams="--color auto -al a" call :test
set setParams="--color auto -cal a" call :test
set setParams="--color auto -cl a" call :test

set setParams="--color auto -v a" call :test
set setParams="--color auto -v -a a" call :test
set setParams="--color auto -v -c a" call :test
set setParams="--color auto -v -ca a" call :test
set setParams="--color auto -v -l a" call :test
set setParams="--color auto -v -al a" call :test
set setParams="--color auto -v -cal a" call :test
set setParams="--color auto -v -cl a" call :test

set setParams="--color auto --context 5 a" call :test
set setParams="--color auto --context 5 -a a" call :test
set setParams="--color auto --context 5 -c a" call :test
set setParams="--color auto --context 5 -ca a" call :test
set setParams="--color auto --context 1 -l a" call :test
set setParams="--color auto --context 1 -al a" call :test
set setParams="--color auto --context 1 -cal a" call :test
set setParams="--color auto --context 1 -cl A" call :test

set setParams="--color auto --context 5 -v a" call :test
set setParams="--color auto --context 5 -v -a a" call :test
set setParams="--color auto --context 1 -v -c a" call :test
set setParams="--color auto --context 1 -v -ca a" call :test
set setParams="--color auto --context 1 -v -l a" call :test
set setParams="--color auto --context 1 -v -al a" call :test
set setParams="--color auto --context 1 -v -cal a" call :test
set setParams="--color auto --context 1 -v -cl a" call :test

set setParams="--color auto --only-line-nums a" call :test
set setParams="--color auto --only-line-nums -a a" call :test
set setParams="--color auto --only-line-nums -c a" call :test
set setParams="--color auto --only-line-nums -ca a" call :test
set setParams="--color auto --only-line-nums -l a" call :test
set setParams="--color auto --only-line-nums -al a" call :test
set setParams="--color auto --only-line-nums -cal a" call :test
set setParams="--color auto --only-line-nums -cl a" call :test

set setParams="--color auto --only-line-nums -v a" call :test
set setParams="--color auto --only-line-nums -v -a a" call :test
set setParams="--color auto --only-line-nums -v -c a" call :test
set setParams="--color auto --only-line-nums -v -ca a" call :test
set setParams="--color auto --only-line-nums -v -l a" call :test
set setParams="--color auto --only-line-nums -v -al a" call :test
set setParams="--color auto --only-line-nums -v -cal a" call :test
set setParams="--color auto --only-line-nums -v -cl a" call :test

set setParams="--color auto --only-line-nums --context 5 a" call :test
set setParams="--color auto --only-line-nums --context 5 -a a" call :test
set setParams="--color auto --only-line-nums --context 5 -c a" call :test
set setParams="--color auto --only-line-nums --context 5 -ca a" call :test
set setParams="--color auto --only-line-nums --context 1 -l a" call :test
set setParams="--color auto --only-line-nums --context 1 -al a" call :test
set setParams="--color auto --only-line-nums --context 1 -cal a" call :test
set setParams="--color auto --only-line-nums --context 1 -cl A" call :test

set setParams="--color auto --only-line-nums --context 5 -v a" call :test
set setParams="--color auto --only-line-nums --context 5 -v -a a" call :test
set setParams="--color auto --only-line-nums --context 1 -v -c a" call :test
set setParams="--color auto --only-line-nums --context 1 -v -ca a" call :test
set setParams="--color auto --only-line-nums --context 1 -v -l a" call :test
set setParams="--color auto --only-line-nums --context 1 -v -al a" call :test
set setParams="--color auto --only-line-nums --context 1 -v -cal a" call :test
set setParams="--color auto --only-line-nums --context 1 -v -cl a" call :test

set setParams="" call :test

set setParams="a a" call :test
set setParams="a --context 5" call :test
set setParams="--context 1000000 a" call :test
set setParams="--color invalidvalue a" call :test
