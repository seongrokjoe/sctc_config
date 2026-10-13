param([Parameter(Mandatory = $true)][string]$ExePath)
$ErrorActionPreference = 'Stop'
$ExePath = (Resolve-Path -LiteralPath $ExePath).Path
$fixtureParent = Join-Path (Split-Path -Parent $PSScriptRoot) 'build'
New-Item -ItemType Directory -Force -Path $fixtureParent | Out-Null
$fixture = Join-Path $fixtureParent ('integration_' + [Guid]::NewGuid().ToString('N'))
$utf8 = New-Object System.Text.UTF8Encoding($false)
$lf = [string][char]10
$crlf = [string][char]13 + $lf
$header = 'Number,Label,Level,Group,Unit,Part,Message,RunFlag,SetupFlag,DefaultFlag'
$passed = 0

function Assert-True([bool]$Value, [string]$Message) {
    if (!$Value) { throw $Message }
}
function Row([string]$Id, [string]$Run = 'DisplayOnly:1', [string]$Setup = 'Setup:2', [string]$Default = 'Default:3', [string]$Description = 'Description') {
    $values = @($Id, 'Name', 'WARN', 'Category', 'Module', 'Sub', $Description, $Run, $Setup, $Default)
    return (($values | ForEach-Object {
        if ($_.Contains(',') -or $_.Contains('"')) { '"' + $_.Replace('"', '""') + '"' } else { $_ }
    }) -join ',')
}
function Put-Text([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText($Path, $Text, $utf8)
}
function Tree-Hash([string]$Path) {
    $entries = @(Get-ChildItem -LiteralPath $Path -Recurse -Force | Sort-Object FullName | ForEach-Object {
        $relative = $_.FullName.Substring($Path.Length)
        if ($_.PSIsContainer) { "D $relative" } else { "F $relative $((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash)" }
    })
    return ($entries -join $lf)
}
function Start-Console([string]$Program) {
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $Program
    $info.WorkingDirectory = $fixture
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    $info.RedirectStandardInput = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.StandardOutputEncoding = $utf8
    $info.StandardErrorEncoding = $utf8
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $info
    [void]$process.Start()
    return @{ Process = $process; Stdout = $process.StandardOutput.ReadToEndAsync(); Stderr = $process.StandardError.ReadToEndAsync() }
}
function Send-Input($Session, [string]$Text, [bool]$Close = $true) {
    $bytes = $utf8.GetBytes($Text)
    $Session.Process.StandardInput.BaseStream.Write($bytes, 0, $bytes.Length)
    $Session.Process.StandardInput.BaseStream.Flush()
    if ($Close) { $Session.Process.StandardInput.Close() }
}
function End-Console($Session) {
    if (!$Session.Process.WaitForExit(30000)) { $Session.Process.Kill(); throw 'Console execution timed out.' }
    $result = @{ Code = $Session.Process.ExitCode; Out = $Session.Stdout.Result; Err = $Session.Stderr.Result }
    $Session.Process.Dispose()
    return $result
}
function Invoke-Console([string]$Program, [string]$InputText) {
    $session = Start-Console $Program
    Send-Input $session $InputText
    return End-Console $session
}
function Passed([string]$Name) {
    $script:passed++
    Write-Host "PASS $Name"
}

try {
    $old = Join-Path $fixture 'OLD 장비'
    $new = Join-Path $fixture 'NEW 패치'
    $tools = Join-Path $fixture '실행 도구'
    foreach ($dir in @($old, $new, $tools, "$old\sub", "$new\sub", "$new\empty")) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }
    $exe = Join-Path $tools 'AlarmCsvMerge.exe'
    Copy-Item -LiteralPath $ExePath -Destination $exe
    Put-Text "$old\BM.csv" ($header + $crlf + (Row '2002') + $crlf + (Row '1001' 'EMGStop:5|6' 'SetupOld:2' '' 'old') + $crlf + (Row '9000'))
    $newBm = $header + $lf + (Row '1001' 'DisplayOnly:1' 'SetupNew:2' 'Default:3' 'new, description with ""quotes""') + $lf + (Row '3003') + $lf + (Row '2002')
    $expectedBm = $header + $lf + (Row '1001' 'EMGStop:5|6' 'SetupOld:2' '' 'new, description with ""quotes""') + $lf + (Row '3003') + $lf + (Row '2002')
    Put-Text "$new\BM.csv" $newBm
    Put-Text "$old\OldOnly.csv" ($header + $lf + (Row 'OLD'))
    Put-Text "$new\NewOnly.csv" ($header + $lf + (Row 'NEW'))
    Put-Text "$old\sub\BM.csv" ($header + $lf + (Row '1001' 'SUB_OLD'))
    Put-Text "$new\sub\bm.CSV" ($header + $lf + (Row '1001' 'SUB_NEW'))
    Put-Text "$old\Duplicate.csv" ($header + $lf + (Row 'A' 'OLD') + $lf + (Row 'A' 'OTHER') + $lf + (Row 'B' 'OLD'))
    Put-Text "$new\Duplicate.csv" ($header + $lf + (Row 'A') + $lf + (Row 'B'))
    Put-Text "$new\Malformed.csv" ($header + $lf + 'A,short')
    [IO.File]::WriteAllBytes("$new\payload.bin", [byte[]](0, 255, 129, 0, 13, 10))
    $beforeOld = Tree-Hash $old
    $beforeNew = Tree-Hash $new

    $response = Invoke-Console $exe ('"' + $old + '"' + $lf + '"' + $new + '"' + $lf + 'N' + $lf)
    Assert-True ($response.Code -eq 2) "N exit code: $($response.Code) $($response.Err)"
    Assert-True (!(Test-Path -LiteralPath "$tools\Alarm")) 'N must not generate an Alarm folder.'
    $reports = @(Get-ChildItem -LiteralPath $tools -Filter 'AlarmDiff_*.txt')
    Assert-True ($reports.Count -eq 1) 'N must write exactly one report.'
    $report = [IO.File]::ReadAllText($reports[0].FullName)
    foreach ($token in @('Path1 ONLY', 'Path2 ONLY', 'OLD ONLY ID', 'NEW ONLY ID', 'StopMode_Run', 'EMGStop:5|6', 'OTHER COLUMN', 'Duplicate ID', 'AUTOCHANGE NOT REQUESTED')) {
        Assert-True ($report.Contains($token)) "Missing report token: $token"
    }
    $inventory = ($report -split "=== FILE INVENTORY ===", 2)[1] -split "=== CSV DETAILS ===", 2
    Assert-True (!$report.Contains("Rules:") -and !$report.Contains("CSV layout:")) "Rules block must be absent."
    Assert-True (!$report.Contains("unsupported core columns")) "Arbitrary header names must be accepted."
    Assert-True (!$inventory[0].Contains("BM.csv") -and !$inventory[0].Contains("COMMON FILE")) "Inventory must omit common files."
    Assert-True ($inventory[0].Contains("[Path1 ONLY] OldOnly.csv") -and $inventory[0].Contains("[Path2 ONLY] NewOnly.csv")) "Inventory must identify the side."
    $reportBytes = [IO.File]::ReadAllBytes($reports[0].FullName)
    Assert-True ($reportBytes[0] -eq 239 -and $reportBytes[1] -eq 187 -and $reportBytes[2] -eq 191) 'Report must have UTF-8 BOM.'
    Passed 'N: report, exact ID differences, no output folder'

    $response = Invoke-Console $exe ($old + $lf + $new + $lf + 'Y' + $lf)
    Assert-True ($response.Code -eq 2) "Y exit code: $($response.Code) $($response.Err)"
    Assert-True ([IO.File]::ReadAllText("$tools\Alarm\BM.csv") -ceq $expectedBm) 'BM.csv merge mismatch.'
    Assert-True ([IO.File]::ReadAllText("$tools\Alarm\sub\bm.CSV").Contains('SUB_OLD')) 'Relative path/case-insensitive match failed.'
    Assert-True (!(Test-Path -LiteralPath "$tools\Alarm\OldOnly.csv")) 'OLD-only file must not be generated.'
    foreach ($name in @('NewOnly.csv', 'Duplicate.csv', 'Malformed.csv', 'payload.bin')) {
        Assert-True ((Get-FileHash -LiteralPath "$tools\Alarm\$name").Hash -eq (Get-FileHash -LiteralPath "$new\$name").Hash) "Copy must preserve all bytes: $name"
    }
    Assert-True (Test-Path -LiteralPath "$tools\Alarm\empty" -PathType Container) 'Empty directory missing.'
    $report = [IO.File]::ReadAllText((Get-ChildItem -LiteralPath $tools -Filter 'AlarmDiff_*.txt' | Sort-Object LastWriteTimeUtc | Select-Object -Last 1).FullName)
    Assert-True ($report.Contains('[AUTOCHANGE COMPLETE]') -and $report.Contains('Modified=2') -and $report.Contains('Excluded=2')) 'Generation summary incorrect.'
    Passed 'Y: merge, nested files, case matching, exclusions, binary and empty directory copy'

    $firstOutputHash = Tree-Hash "$tools\Alarm"
    $response = Invoke-Console $exe ($old + $lf + $new + $lf + 'Y' + $lf)
    Assert-True ($response.Code -eq 2) 'Repeated Y should succeed with exclusions.'
    Assert-True ((Tree-Hash "$tools\Alarm") -ceq $firstOutputHash) 'Existing Alarm output was overwritten.'
    Assert-True (@(Get-ChildItem -LiteralPath $tools -Directory -Filter 'Alarm_*').Count -eq 1) 'Timestamp result directory missing.'
    Assert-True (@(Get-ChildItem -LiteralPath $tools -Filter 'AlarmDiff_*.txt').Count -eq 3) 'Reports must not overwrite.'
    Passed 'Repeated run: previous folder and report preserved'

    $response = Invoke-Console $exe ($old + $lf + $new + $lf + 'invalid' + $lf + 'n' + $lf)
    Assert-True ($response.Code -eq 2 -and $response.Out.Contains('Y 또는 N')) 'Invalid answer must re-prompt.'
    $response = Invoke-Console $exe ($old + $lf + $new + $lf)
    Assert-True ($response.Code -eq 2) 'EOF should act as N.'
    Passed 'Y/N validation and EOF cancellation'

    $lock = [IO.File]::Open("$old\BM.csv", [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try { $response = Invoke-Console $exe ($old + $lf + $new + $lf + 'Y' + $lf) } finally { $lock.Dispose() }
    Assert-True ($response.Code -eq 1 -and $response.Out.Contains('DISABLED')) 'Unreadable input must disable generation.'
    Assert-True (@(Get-ChildItem -LiteralPath $tools -Directory -Filter 'Alarm*').Count -eq 2) 'Failed inventory must not generate output.'
    Passed 'Read failure: diff recorded, generation disabled'

    $response = Invoke-Console $exe ($tools + $lf + $new + $lf + 'Y' + $lf)
    Assert-True ($response.Code -eq 1) 'Executable inside input must be rejected.'
    Assert-True ((Tree-Hash $old) -ceq $beforeOld) 'OLD source changed.'
    Assert-True ((Tree-Hash $new) -ceq $beforeNew) 'NEW source changed.'
    Passed 'Input containment protection and source hash preservation'

    $snapshotTools = Join-Path $fixture 'snapshot-tools'
    New-Item -ItemType Directory -Path $snapshotTools | Out-Null
    $snapshotExe = Join-Path $snapshotTools 'AlarmCsvMerge.exe'
    Copy-Item -LiteralPath $ExePath -Destination $snapshotExe
    $session = Start-Console $snapshotExe
    try {
        Send-Input $session ($old + $lf + $new + $lf) $false
        $deadline = [DateTime]::UtcNow.AddSeconds(20)
        while (!(Get-ChildItem -LiteralPath $snapshotTools -Filter 'AlarmDiff_*.txt')) {
            if ($session.Process.HasExited -or [DateTime]::UtcNow -gt $deadline) { throw 'Comparison did not reach report stage.' }
            Start-Sleep -Milliseconds 50
        }
        Put-Text "$new\BM.csv" ($header + $lf + (Row 'CHANGED_ON_DISK'))
        Send-Input $session ('Y' + $lf)
        $response = End-Console $session
        $session = $null
        Assert-True ($response.Code -eq 2) 'Snapshot generation failed.'
        Assert-True ([IO.File]::ReadAllText("$snapshotTools\Alarm\BM.csv") -ceq $expectedBm) 'Generation must use compared snapshot.'
    } finally {
        Put-Text "$new\BM.csv" $newBm
        if ($null -ne $session) {
            if (!$session.Process.HasExited) { $session.Process.Kill(); $session.Process.WaitForExit() }
            $session.Process.Dispose()
        }
    }
    Assert-True ((Tree-Hash $new) -ceq $beforeNew) 'Snapshot test did not restore its fixture.'
    Passed 'Compared input snapshot reused after Y/N wait'
    Assert-True (@(Get-ChildItem -LiteralPath $fixture -Recurse -Directory -Filter 'Alarm.pending_*').Count -eq 0) 'Temporary output leaked.'
    Write-Host "$passed integration scenarios passed."
} finally {
    if (Test-Path -LiteralPath $fixture) {
        $resolvedFixture = (Resolve-Path -LiteralPath $fixture).Path
        $resolvedParent = (Resolve-Path -LiteralPath $fixtureParent).Path.TrimEnd('\') + '\'
        if (!$resolvedFixture.StartsWith($resolvedParent, [StringComparison]::OrdinalIgnoreCase) -or
            !(Split-Path -Leaf $resolvedFixture).StartsWith('integration_')) {
            throw "Refusing to delete unexpected test path: $resolvedFixture"
        }
        # Windows/virus scanners can briefly retain the image mapping after process exit.
        for ($attempt = 0; ; $attempt++) {
            try {
                Remove-Item -LiteralPath $resolvedFixture -Recurse -Force -ErrorAction Stop
                break
            } catch {
                if ($attempt -ge 19) { throw }
                Start-Sleep -Milliseconds 500
            }
        }
    }
}
