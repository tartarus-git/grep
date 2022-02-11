Get-ChildItem ./templates | Foreach-Object {
    Get-Content $_.FullName | Out-File -FilePath $_.FullName -Encoding ascii
}