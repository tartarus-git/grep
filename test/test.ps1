function validateTestResult {
    param($template, $testResult)
    if (!(git diff --no-index $template $testResult)) { Write-Host "test passed"; return }
    Write-Host "TEST FAILED"
    git diff --no-index $template $testResult
}

$global:index = 0
function test {
    Write-Host -NoNewline "$($global:index)  "
    cat ./inputs/source.cpp | ../Debug/grep.exe $args > output.txt
    validateTestResult ./templates/$($global:index).txt ./output.txt
    $global:index++
}

test a
test -a a
test -c a
test -ca a
test -l a
test -al a
test -cal a
test -cl a

test -v a
test -v -a a
test -v -c a
test -v -ca a
test -v -l a
test -v -al a
test -v -cal a
test -v -cl a

test --context 5 a
test --context 5 -a a
test --context 5 -c a
test --context 5 -ca a
test --context 1 -l a
test --context 1 -al a
test --context 1 -cal a
test --context 1 -cl A

test --context 5 -v a
#test --context 5 -v -a a
#test --context 5 -v -c a
#test --context 5 -v -ca a
#test --context 5 -v -l a
#test --context 5 -v -al a
#test --context 5 -v -cal a
#test --context 5 -v -cl a