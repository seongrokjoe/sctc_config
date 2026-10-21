param([Parameter(Mandatory = $true)][string]$ExePath)
$ErrorActionPreference = 'Stop'
$ExePath = (Resolve-Path -LiteralPath $ExePath).Path
if ((Split-Path -Leaf $ExePath) -cne 'SCTC_Alarm_Compare.exe') { throw 'Expected build output SCTC_Alarm_Compare.exe.' }
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
function Report-Section([string]$Text, [string]$Title, [string]$Next) {
    $first = $Text.IndexOf($Title, [StringComparison]::Ordinal)
    Assert-True ($first -ge 0) "Missing report category: $Title"
    $start = $first + $Title.Length
    $end = $Text.IndexOf($Next, $start, [StringComparison]::Ordinal)
    Assert-True ($end -ge 0) "Missing or out-of-order report category: $Next"
    return $Text.Substring($start, $end - $start)
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
function Invoke-Compare([string]$Program, [string]$InputText) {
    return Invoke-Console $Program ('1' + $lf + $InputText)
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
    $exe = Join-Path $tools 'SCTC_Alarm_Compare.exe'
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
    $unchangedNames = @('Unchanged.csv', 'Equivalent.csv', 'QuoteEquivalent.csv', 'HeaderOnly.csv', 'common.bin')
    foreach ($dir in @($old, $new)) {
        Put-Text "$dir\Unchanged.csv" ($header + $lf + (Row 'SAME'))
        Put-Text "$dir\HeaderOnly.csv" ($header + $lf)
        [IO.File]::WriteAllBytes("$dir\common.bin", [byte[]](0, 255, 13, 10))
    }
    Put-Text "$old\Equivalent.csv" ($header + $crlf + (Row 'B') + $crlf + (Row 'A' 'DisplayOnly:1' 'Setup:2' 'Default:3' '  Description  '))
    Put-Text "$new\Equivalent.csv" ($header.Replace('Number,Label', 'ID,Name') + $lf + (Row 'A') + $lf + (Row 'B'))
    Put-Text "$old\QuoteEquivalent.csv" ($header + $lf + 'E,Name,WARN,Category,Module,Sub,abc""def,DisplayOnly:1,Setup:2,Default:3')
    Put-Text "$new\QuoteEquivalent.csv" ($header + $lf + (Row 'E' 'DisplayOnly:1' 'Setup:2' 'Default:3' 'abc"def'))
    Put-Text "$old\Quotes.csv" ($header + $lf + 'Q,Name,WARN,Category,Module,Sub,"memo, ""quoted""",EMG""stop,""Setup"",')
    Put-Text "$new\Quotes.csv" ($header + $crlf + 'Q,Name,WARN,Category,Module,Sub,memo""new,NEW,NEWSETUP,NEWDEF')
    $expectedQuotes = $header + $crlf + 'Q,Name,WARN,Category,Module,Sub,memo""new,EMG""stop,""Setup"",'
    $beforeOld = Tree-Hash $old
    $beforeNew = Tree-Hash $new

    $response = Invoke-Compare $exe ('"' + $old + '"' + $lf + '"' + $new + '"' + $lf + 'N' + $lf)
    Assert-True ($response.Code -eq 2) "N exit code: $($response.Code) $($response.Err)"
    Assert-True (!(Test-Path -LiteralPath "$tools\Alarm")) 'N must not generate an Alarm folder.'
    $reports = @(Get-ChildItem -LiteralPath $tools -Filter 'AlarmDiff_*.txt')
    Assert-True ($reports.Count -eq 1) 'N must write exactly one report.'
    $report = [IO.File]::ReadAllText($reports[0].FullName)
    foreach ($token in @('Path1 ONLY', 'Path2 ONLY', 'EQ Only(PATH1)', 'PatchSet Only(PATH2)', 'StopMode_Run', 'EMGStop:5|6', 'OTHER COLUMN', 'Duplicate ID', 'AUTOCHANGE NOT REQUESTED')) {
        Assert-True ($report.Contains($token)) "Missing report token: $token"
    }
    $inventory = Report-Section $report '<알람 파일 List 차이>' '<파일별 StopMode 차이>'
    $stopModes = Report-Section $report '<파일별 StopMode 차이>' '<파일별 StopMode는 동일하지만 TreatMode가 다른 리스트>'
    $treatModes = Report-Section $report '<파일별 StopMode는 동일하지만 TreatMode가 다른 리스트>' '<파일별로 Alarm ID 기준으로 한쪽에만 있는 리스트>'
    Assert-True ($treatModes.Contains('차이 없음')) 'Empty TreatMode category is required.'
    Assert-True (!$response.Out.Contains('적용할까요?')) 'AutoChange N must not ask the TreatMode question.'
    $onlyIds = Report-Section $report '<파일별로 Alarm ID 기준으로 한쪽에만 있는 리스트>' '<파일별 Alarm ID 기준으로 컬럼별 내용이 다른 리스트>'
    $otherColumns = Report-Section $report '<파일별 Alarm ID 기준으로 컬럼별 내용이 다른 리스트>' '=== 오류 및 비교 제외 내역 ==='
    $diagnostics = Report-Section $report '=== 오류 및 비교 제외 내역 ===' '=== SUMMARY ==='
    Assert-True (!$report.Contains("Rules:") -and !$report.Contains("CSV layout:")) "Rules block must be absent."
    Assert-True (!$report.Contains("unsupported core columns")) "Arbitrary header names must be accepted."
    Assert-True (!$inventory.Contains("BM.csv") -and !$inventory.Contains("COMMON FILE") -and !$report.Contains('payload.bin')) "Inventory must omit common and non-CSV files."
    Assert-True ($inventory.Contains("[Path1 ONLY]" + $crlf + "  OldOnly.csv") -and $inventory.Contains("[Path2 ONLY]" + $crlf + "  Malformed.csv" + $crlf + "  NewOnly.csv")) "Inventory must group and sort files by side."
    Assert-True ($stopModes.Contains('--- File: BM.csv ---') -and $stopModes.Contains('--- File: sub\BM.csv ---') -and !$stopModes.Contains('OTHER COLUMN')) 'StopMode category mismatch.'
    Assert-True ($onlyIds.Contains('[EQ Only(PATH1)] ID="9000" Name="Name" Line=4') -and $onlyIds.Contains('[PatchSet Only(PATH2)] ID="3003" Name="Name" Line=3')) 'ID list must contain side prefix, ID, Name and physical Line.'
    Assert-True (!$onlyIds.Contains('OldOnly.csv') -and !$onlyIds.Contains('NewOnly.csv') -and !$onlyIds.Contains('sub\BM.csv')) 'ID lists must omit whole-file and StopMode-only differences.'
    Assert-True ($otherColumns.Contains('--- File: BM.csv ---') -and !$otherColumns.Contains('sub\BM.csv') -and !$otherColumns.Contains('[STOPMODE]')) 'Other columns must exclude StopModes.'
    Assert-True ($diagnostics.Contains('Duplicate.csv') -and $diagnostics.Contains('Malformed.csv') -and $diagnostics.Contains('[ERROR Path1]')) 'Errors must appear in separate diagnostics.'
    foreach ($section in @($stopModes, $treatModes, $onlyIds, $otherColumns)) {
        Assert-True (!$section.Contains('Duplicate.csv') -and !$section.Contains('Malformed.csv')) 'Invalid CSVs must not enter value or ID categories.'
    }
    Assert-True ($stopModes.Contains('Quotes.csv') -and $otherColumns.Contains('Quotes.csv') -and !$diagnostics.Contains('Quotes.csv')) 'Doubled quotes must compare normally.'
    foreach ($name in $unchangedNames) {
        Assert-True (!$report.Contains($name)) "Unchanged file name must be absent: $name"
    }
    Assert-True (!$report.Contains('NO VALUE DIFFERENCES')) 'No-difference markers must be absent.'
    Assert-True ($report.Contains('CSV union=11')) 'Summary must count omitted CSV files.'
    Assert-True ($report.Contains('--- File: BM.csv ---') -and $report.Contains('--- File: Duplicate.csv ---')) 'Differences and errors must remain visible.'
    $reportBytes = [IO.File]::ReadAllBytes($reports[0].FullName)
    Assert-True ($reportBytes[0] -eq 239 -and $reportBytes[1] -eq 187 -and $reportBytes[2] -eq 191) 'Report must have UTF-8 BOM.'
    Passed 'N: report, exact ID differences, no output folder'

    $response = Invoke-Compare $exe ($old + $lf + $new + $lf + 'Y' + $lf + 'N' + $lf)
    Assert-True ($response.Code -eq 2) "Y exit code: $($response.Code) $($response.Err)"
    Assert-True ($response.Out.Contains('대상 0건') -and $response.Out.Contains('설명용 예제') -and $response.Out.Contains('적용할까요?')) 'Zero targets still show an illustrative example and question.'
    Assert-True ([IO.File]::ReadAllText("$tools\Alarm\BM.csv") -ceq $expectedBm) 'BM.csv merge mismatch.'
    Assert-True ([IO.File]::ReadAllText("$tools\Alarm\Quotes.csv") -ceq $expectedQuotes) 'Doubled quotes and raw StopMode spans must survive AutoChange.'
    Assert-True ([IO.File]::ReadAllText("$tools\Alarm\sub\bm.CSV").Contains('SUB_OLD')) 'Relative path/case-insensitive match failed.'
    Assert-True (!(Test-Path -LiteralPath "$tools\Alarm\OldOnly.csv")) 'OLD-only file must not be generated.'
    foreach ($name in (@('NewOnly.csv', 'Duplicate.csv', 'Malformed.csv', 'payload.bin') + $unchangedNames)) {
        Assert-True ((Get-FileHash -LiteralPath "$tools\Alarm\$name").Hash -eq (Get-FileHash -LiteralPath "$new\$name").Hash) "Copy must preserve all bytes: $name"
    }
    Assert-True (Test-Path -LiteralPath "$tools\Alarm\empty" -PathType Container) 'Empty directory missing.'
    $report = [IO.File]::ReadAllText((Get-ChildItem -LiteralPath $tools -Filter 'AlarmDiff_*.txt' | Sort-Object LastWriteTimeUtc | Select-Object -Last 1).FullName)
    Assert-True ($report.Contains('[AUTOCHANGE COMPLETE]') -and $report.Contains('Modified=3') -and $report.Contains('Excluded=2')) 'Generation summary incorrect.'
    Assert-True ($report.Contains('TreatMode-only 적용 선택: N') -and $report.Contains('변경 전 PatchSet : ') -and $report.Contains('변경 후 출력 Mode : ')) 'Selection and before/after labels are required.'
    Assert-True (!$report.Contains('  NEW:') -and !$report.Contains('  OUTPUT:') -and !$report.Contains('[FORMAT ONLY]')) 'Ambiguous labels and formatting-only changes must be absent.'
    foreach ($name in $unchangedNames) {
        Assert-True (!$report.Contains($name)) "Unchanged file must stay absent after AutoChange: $name"
    }
    Assert-True (!$report.Contains('[UNCHANGED]') -and !$report.Contains('NO VALUE DIFFERENCES')) 'AutoChange must omit unchanged-file details.'
    Passed 'Y: merge, nested files, case matching, exclusions, binary and empty directory copy'

    $firstOutputHash = Tree-Hash "$tools\Alarm"
    $response = Invoke-Compare $exe ($old + $lf + $new + $lf + 'Y' + $lf)
    Assert-True ($response.Code -eq 2) 'Repeated Y should succeed with exclusions.'
    Assert-True ((Tree-Hash "$tools\Alarm") -ceq $firstOutputHash) 'Existing Alarm output was overwritten.'
    Assert-True (@(Get-ChildItem -LiteralPath $tools -Directory -Filter 'Alarm_*').Count -eq 1) 'Timestamp result directory missing.'
    Assert-True (@(Get-ChildItem -LiteralPath $tools -Filter 'AlarmDiff_*.txt').Count -eq 3) 'Reports must not overwrite.'
    Passed 'Repeated run: previous folder and report preserved'

    $response = Invoke-Compare $exe ($old + $lf + $new + $lf + 'invalid' + $lf + 'n' + $lf)
    Assert-True ($response.Code -eq 2 -and $response.Out.Contains('Y 또는 N')) 'Invalid answer must re-prompt.'
    $response = Invoke-Compare $exe ($old + $lf + $new + $lf)
    Assert-True ($response.Code -eq 2) 'EOF should act as N.'
    Passed 'Y/N validation and EOF cancellation'

    $lock = [IO.File]::Open("$old\BM.csv", [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try { $response = Invoke-Compare $exe ($old + $lf + $new + $lf + 'Y' + $lf) } finally { $lock.Dispose() }
    Assert-True ($response.Code -eq 1 -and $response.Out.Contains('DISABLED')) 'Unreadable input must disable generation.'
    Assert-True (@(Get-ChildItem -LiteralPath $tools -Directory -Filter 'Alarm*').Count -eq 2) 'Failed inventory must not generate output.'
    Passed 'Read failure: diff recorded, generation disabled'

    $response = Invoke-Compare $exe ($tools + $lf + $new + $lf + 'Y' + $lf)
    Assert-True ($response.Code -eq 1) 'Executable inside input must be rejected.'
    Assert-True ((Tree-Hash $old) -ceq $beforeOld) 'OLD source changed.'
    Assert-True ((Tree-Hash $new) -ceq $beforeNew) 'NEW source changed.'
    Passed 'Input containment protection and source hash preservation'

    $snapshotTools = Join-Path $fixture 'snapshot-tools'
    New-Item -ItemType Directory -Path $snapshotTools | Out-Null
    $snapshotExe = Join-Path $snapshotTools 'SCTC_Alarm_Compare.exe'
    Copy-Item -LiteralPath $ExePath -Destination $snapshotExe
    $session = Start-Console $snapshotExe
    try {
        Send-Input $session ('1' + $lf + $old + $lf + $new + $lf) $false
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

    $modeOld = Join-Path $fixture 'mode-old'
    $modeNew = Join-Path $fixture 'mode-new'
    foreach ($dir in @($modeOld, $modeNew)) { New-Item -ItemType Directory -Path $dir | Out-Null }
    $modeEq = $header + $lf + 'A,Name,WARN,Category,Module,Sub,Description,"EMGStop:4|5|6","Ignore:0","Same : 9 "'
    $modePrefix = $header + $crlf + 'A,Name,WARN,Category,Module,Sub,Description,'
    $modePatch = $modePrefix + 'DisplayOnly:1,Ignore:4|5|6,Same:9'
    Put-Text "$modeOld\Modes.csv" $modeEq
    Put-Text "$modeNew\Modes.csv" $modePatch
    Put-Text "$modeOld\Treat.csv" ($header + $lf + (Row 'Z' 'Ignore:1') + $lf + (Row 'A' 'Ignore:2'))
    $treatPatch = $header + $lf + (Row 'Z' 'Ignore:0') + $lf + (Row 'A' 'Ignore:0')
    Put-Text "$modeNew\Treat.csv" $treatPatch
    Put-Text "$modeOld\Format.csv" ($header + $lf + 'F,Name,WARN,Category,Module,Sub,Description,"Ignore:0","Setup:2","Default:3"')
    $formatPatch = $header + $lf + (Row 'F' ' Ignore : 0 ')
    Put-Text "$modeNew\Format.csv" $formatPatch
    $modeOldHash = Tree-Hash $modeOld
    $modeNewHash = Tree-Hash $modeNew
    $choices = @(
        @{ Name = 'no'; Input = 'Y' + $lf + ' n ' + $lf; Include = $false },
        @{ Name = 'yes'; Input = 'y' + $lf + 'invalid' + $lf + ' Y ' + $lf; Include = $true },
        @{ Name = 'eof'; Input = 'Y' + $lf; Include = $false }
    )
    foreach ($choice in $choices) {
        $modeTools = Join-Path $fixture ('mode-tools-' + $choice.Name)
        New-Item -ItemType Directory -Path $modeTools | Out-Null
        $modeExe = Join-Path $modeTools 'SCTC_Alarm_Compare.exe'
        Copy-Item -LiteralPath $ExePath -Destination $modeExe
        $response = Invoke-Compare $modeExe ($modeOld + $lf + $modeNew + $lf + $choice.Input)
        Assert-True ($response.Code -eq 0) "Mode choice $($choice.Name) failed: $($response.Err)"
        Assert-True ($response.Out.Contains('대상 3건') -and $response.Out.Contains('File: Modes.csv') -and $response.Out.Contains('[TREATMODE] ID="A" Column=9') -and $response.Out.Contains('Y 선택 시 출력 Mode') -and $response.Out.Contains('N 선택 시 출력 Mode')) 'Actual example must use first sorted file/ID/column.'
        $selectedRun = '"EMGStop:4|5|6",'
        if ($choice.Include) { $selectedRun += '"Ignore:0",Same:9' } else { $selectedRun += 'Ignore:4|5|6,Same:9' }
        Assert-True ([IO.File]::ReadAllText("$modeTools\Alarm\Modes.csv") -ceq ($modePrefix + $selectedRun)) 'Each mode column must obey its own classification and selection.'
        $expectedTreat = if ($choice.Include) { [IO.File]::ReadAllText("$modeOld\Treat.csv") } else { $treatPatch }
        Assert-True ([IO.File]::ReadAllText("$modeTools\Alarm\Treat.csv") -ceq $expectedTreat) 'Treat-only file selection mismatch.'
        Assert-True ((Get-FileHash -LiteralPath "$modeTools\Alarm\Format.csv").Hash -eq (Get-FileHash -LiteralPath "$modeNew\Format.csv").Hash) 'Formatting-only fields must remain byte-identical for both choices.'
        $modeReport = [IO.File]::ReadAllText((Get-ChildItem -LiteralPath $modeTools -Filter 'AlarmDiff_*.txt').FullName)
        $stopSection = Report-Section $modeReport '<파일별 StopMode 차이>' '<파일별 StopMode는 동일하지만 TreatMode가 다른 리스트>'
        $treatSection = Report-Section $modeReport '<파일별 StopMode는 동일하지만 TreatMode가 다른 리스트>' '<파일별로 Alarm ID 기준으로 한쪽에만 있는 리스트>'
        Assert-True (!$stopSection.Contains('Treat.csv') -and $treatSection.Contains('Modes.csv') -and $treatSection.Contains('Treat.csv')) 'Mode reports must split categories independently of the answer.'
        Assert-True (!$modeReport.Contains('Format.csv') -and $modeReport.Contains('Different common IDs=3 StopMode cells=1 TreatMode-only cells=3')) 'Summary counts and formatting omissions must be accurate.'
        $selection = if ($choice.Include) { 'Y' } else { 'N' }
        Assert-True ($modeReport.Contains('TreatMode-only 적용 선택: ' + $selection)) 'Selected policy must be recorded.'
        if ($choice.Include) { Assert-True ($response.Out.Contains('Y 또는 N')) 'Invalid second answer must re-prompt.' }
    }
    Assert-True ((Tree-Hash $modeOld) -ceq $modeOldHash) 'Mode EQ input changed.'
    Assert-True ((Tree-Hash $modeNew) -ceq $modeNewHash) 'Mode PatchSet input changed.'
    Passed 'TreatMode Y/N/EOF, real example, per-column selection and formatting preservation'

    $encodingOld = Join-Path $fixture 'encoding-old'
    $encodingNew = Join-Path $fixture 'encoding-new'
    foreach ($dir in @($encodingOld, $encodingNew)) { New-Item -ItemType Directory -Path $dir | Out-Null }
    $emoji = [char]::ConvertFromUtf32(0x1F600)
    Put-Text "$encodingOld\Encoding.csv" ($header + $lf + (Row 'A' ('Ignore:' + $emoji) 'OLD' 'Same' '장비'))
    $cp949 = [Text.Encoding]::GetEncoding(949)
    $encodingPatch = $header + $crlf + (Row 'A' 'Ignore:0' 'NEW' 'Same' '패치')
    [IO.File]::WriteAllBytes("$encodingNew\Encoding.csv", $cp949.GetBytes($encodingPatch))
    foreach ($choice in @('N', 'Y')) {
        $encodingTools = Join-Path $fixture ('encoding-tools-' + $choice)
        New-Item -ItemType Directory -Path $encodingTools | Out-Null
        $encodingExe = Join-Path $encodingTools 'SCTC_Alarm_Compare.exe'
        Copy-Item -LiteralPath $ExePath -Destination $encodingExe
        $response = Invoke-Compare $encodingExe ($encodingOld + $lf + $encodingNew + $lf + 'Y' + $lf + $choice + $lf)
        $expectedCode = if ($choice -eq 'Y') { 2 } else { 0 }
        Assert-True ($response.Code -eq $expectedCode) 'Final exit code must reflect selected output encoding failures.'
        $expectedText = if ($choice -eq 'Y') { $encodingPatch } else { $header + $crlf + (Row 'A' 'Ignore:0' 'OLD' 'Same' '패치') }
        Assert-True ($cp949.GetString([IO.File]::ReadAllBytes("$encodingTools\Alarm\Encoding.csv")) -ceq $expectedText) 'Encoding exclusion must use the selected policy and preserve the entire file on failure.'
        $encodingReport = [IO.File]::ReadAllText((Get-ChildItem -LiteralPath $encodingTools -Filter 'AlarmDiff_*.txt').FullName)
        Assert-True ($encodingReport.Contains('[AUTOCHANGE EXCLUDED]') -eq ($choice -eq 'Y')) 'Encoding diagnostics must match the actual generation choice.'
    }
    Passed 'Selected encoding validation, whole-file fallback and exit code'

    $alignInput = Join-Path $fixture '정렬 대상'
    $alignTools = Join-Path $fixture 'align-tools'
    foreach ($dir in @($alignInput, "$alignInput\sub", "$alignInput\empty", $alignTools)) {
        New-Item -ItemType Directory -Path $dir | Out-Null
    }
    $alignExe = Join-Path $alignTools 'SCTC_Alarm_Compare.exe'
    Copy-Item -LiteralPath $ExePath -Destination $alignExe
    $rowTen = Row '10' ' Ignore:0 ' 'Setup:2' '' '설명, "따옴표"'
    $rowTwo = Row '2'
    $rowOne = Row '001'
    $alignPrefix = '# preamble' + $lf + $header + $lf
    $alignText = $alignPrefix + '# ten' + $lf + $rowTen + $lf + $lf + '# two' + $lf + $rowTwo + $lf + '# one' + $lf + $rowOne + $lf + '# footer'
    $alignExpected = $alignPrefix + '# one' + $lf + $rowOne + $lf + $lf + '# two' + $lf + $rowTwo + $lf + '# ten' + $lf + $rowTen + $lf + '# footer'
    Put-Text "$alignInput\Main.csv" $alignText
    Put-Text "$alignInput\sub\Main.CSV" ($header + $crlf + (Row '100') + $crlf + (Row '2'))
    Put-Text "$alignInput\Sorted.csv" ($header + $crlf + (Row '1') + $crlf + (Row '10') + $crlf)
    Put-Text "$alignInput\Empty.csv" ($header + $lf + '# no data')
    Put-Text "$alignInput\Duplicate.csv" ($header + $lf + (Row '10') + $lf + (Row '2') + $lf + (Row '2'))
    Put-Text "$alignInput\Malformed.csv" ($header + $lf + '1,short')
    [IO.File]::WriteAllBytes("$alignInput\payload.bin", [byte[]](0, 255, 129, 13, 10))
    $alignBefore = Tree-Hash $alignInput
    $response = Invoke-Console $alignExe ('invalid' + $lf + ' 2 ' + $lf + '"' + $alignInput + '"' + $lf)
    Assert-True ($response.Code -eq 2) "Align exit code: $($response.Code) $($response.Err)"
    Assert-True ($response.Out.Contains('1: Compare') -and $response.Out.Contains('2: Align') -and $response.Out.Contains('1 또는 2')) 'Startup menu must validate mode selection.'
    Assert-True ($response.Out.IndexOf('선택 [1/2]') -lt $response.Out.IndexOf('Alarm Folder Path (Align)')) 'Menu must precede path input.'
    Assert-True (!$response.Out.Contains('Alarm Folder Path2') -and !$response.Out.Contains('AutoChange?') -and !$response.Out.Contains('적용할까요?')) 'Align accepts one path without Compare prompts.'
    $alignOutput = Join-Path $alignTools 'Align_정렬 대상'
    Assert-True ([IO.File]::ReadAllText("$alignOutput\Main.csv") -ceq $alignExpected) 'Align must move comments and blanks with the following alarm row, retaining prefix and footer.'
    Assert-True ([IO.File]::ReadAllText("$alignOutput\sub\Main.CSV") -ceq ($header + $crlf + (Row '2') + $crlf + (Row '100'))) 'Nested CSV must retain filename, path, CRLF and no final newline.'
    foreach ($name in @('Sorted.csv', 'Empty.csv', 'Duplicate.csv', 'Malformed.csv', 'payload.bin')) {
        Assert-True ((Get-FileHash -LiteralPath "$alignOutput\$name").Hash -eq (Get-FileHash -LiteralPath "$alignInput\$name").Hash) "Align must preserve original bytes: $name"
    }
    Assert-True (Test-Path -LiteralPath "$alignOutput\empty" -PathType Container) 'Align must preserve empty directories.'
    Assert-True ($response.Out.Contains('CSV 파일=6 정렬=2 미변경=2 제외(원문 복사)=2 비CSV 복사=1') -and $response.Out.Contains('Duplicate.csv') -and $response.Out.Contains('Duplicate ID') -and $response.Out.Contains('Malformed.csv')) 'Align must report actual counts and per-file reasons.'
    Assert-True (@(Get-ChildItem -LiteralPath $alignTools -Filter 'AlarmDiff_*.txt').Count -eq 0) 'Align does not create a comparison report.'
    Assert-True ((Tree-Hash $alignInput) -ceq $alignBefore) 'Align changed source bytes or folder structure.'
    Passed 'Align: numeric order, comment blocks, recursive copy, original bytes and exclusions'

    $firstAlignHash = Tree-Hash $alignOutput
    $response = Invoke-Console $alignExe ('2' + $lf + $alignInput + $lf)
    Assert-True ($response.Code -eq 2 -and (Tree-Hash $alignOutput) -ceq $firstAlignHash) 'Repeated Align must preserve its previous output.'
    Assert-True (@(Get-ChildItem -LiteralPath $alignTools -Directory -Filter 'Align_정렬 대상_*').Count -eq 1) 'Repeated Align must create a unique prefixed directory.'
    $noCsv = Join-Path $fixture 'no-csv'
    New-Item -ItemType Directory -Path $noCsv | Out-Null
    Put-Text "$noCsv\note.txt" 'no alarms'
    $response = Invoke-Console $alignExe ('2' + $lf + $noCsv + $lf)
    Assert-True ($response.Code -eq 0 -and $response.Out.Contains('알람 CSV 파일이 없습니다') -and !(Test-Path -LiteralPath "$alignTools\Align_no-csv")) 'No CSV means no output folder and normal completion.'
    $cleanAlign = Join-Path $fixture 'clean-align'
    New-Item -ItemType Directory -Path $cleanAlign | Out-Null
    Put-Text "$cleanAlign\Clean.csv" ($header + $lf + (Row '10') + $lf + (Row '2'))
    $response = Invoke-Console $alignExe ('2' + $lf + $cleanAlign + $lf)
    Assert-True ($response.Code -eq 0 -and [IO.File]::ReadAllText("$alignTools\Align_clean-align\Clean.csv") -ceq ($header + $lf + (Row '2') + $lf + (Row '10'))) 'Valid Align must exit successfully.'
    Passed 'Align: repeated results, no CSV and clean completion'

    $alignOutputsBeforeFailure = Tree-Hash $alignTools
    foreach ($inputText in @('', ('bad' + $lf), ('2' + $lf), ('2' + $lf + "$fixture\missing" + $lf), ('2' + $lf + $alignTools + $lf))) {
        $response = Invoke-Console $alignExe $inputText
        Assert-True ($response.Code -eq 1) 'Menu/path EOF and invalid/contained paths must abort without generation.'
    }
    $lock = [IO.File]::Open("$alignInput\Main.csv", [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try { $response = Invoke-Console $alignExe ('2' + $lf + $alignInput + $lf) } finally { $lock.Dispose() }
    Assert-True ($response.Code -eq 1 -and $response.Err.Contains('입력 전체를 읽지 못해')) 'Unreadable Align input must abort the entire operation.'
    Assert-True ((Tree-Hash $alignTools) -ceq $alignOutputsBeforeFailure) 'Failed Align must not leave output or modify existing results.'
    Assert-True ((Tree-Hash $alignInput) -ceq $alignBefore) 'Align failure changed its input.'
    Passed 'Align: menu/path validation, read failure and containment protection'

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
