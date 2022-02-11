Get-ChildItem ./templates | Foreach-Object {
    Get-Content $_.FullName | Out-File -FilePath ./tempfile.txt -Encoding ascii
    cp ./tempfile.txt $_.FullName
}