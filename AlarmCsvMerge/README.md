# AlarmCsvMerge

Equipment(Path1, EQ)와 PatchSet(Path2)의 Alarm CSV를 비교하고, PatchSet의 파일·행 구성을 유지하면서 일치하는 ID의 Mode 차이를 EQ 값으로 적용하는 독립 x64 콘솔 프로그램입니다. Run·Setup·Default마다 StopMode 차이와 TreatMode만의 차이를 구분하며, TreatMode만 다른 항목은 사용자 선택에 따라 적용합니다.

실행 메뉴에서 `1: Compare` 또는 `2: Align`을 선택합니다. Align은 폴더 하나의 알람 CSV를 파일별 AlarmID 숫자 오름차순으로 정렬하며, 원본 행 내용은 그대로 유지합니다.

기존 `SCTC_CONFIG.sln`, WPF 소스, 배포 폴더는 사용하거나 변경하지 않습니다. 이 폴더만 복사해 별도로 빌드할 수 있습니다.

## 빌드 및 배포

필요한 개발 도구:

- Visual Studio 2017 또는 v141 도구를 설치한 최신 Visual Studio/Build Tools.
- MSVC v141 C++ x64/x86 빌드 도구.
- Windows SDK **10.0.17134.0**.
- Windows PowerShell 5.1 이상: 빌드·테스트 실행에만 사용합니다.

**`build.bat`을 더블클릭하면 신규 C++ 솔루션만 Release x64로 다시 빌드하고 테스트한 후 아래 파일을 출력합니다.**

```text
AlarmCsvMerge\dist\SCTC_Alarm_Compare.exe
```

이 exe 하나만 장비 PC에 복사합니다. .NET, Python, 별도 설정 파일, 외부 DLL 배포가 필요하지 않습니다. CRT는 `/MT`로 연결합니다. Windows 기본 시스템 DLL은 사용합니다.

빌드 도구가 없으면 필요한 구성 요소를 안내하고 실패합니다. 다른 툴셋으로 자동 변경하거나 설치하지 않습니다. 빌드·테스트 실패 시 새 exe를 dist로 복사하지 않으며, 기존 dist 파일이 있었다면 이전 파일은 남아 있습니다. 최종 `SUCCESS` 메시지를 확인하세요.

배치 파일은 Windows에서 직접 실행 가능한 `.bat` 형식입니다. 실행 호환성을 위해 이 파일만 BOM 없는 UTF-8/CRLF를 사용하며, 나머지 텍스트 파일은 UTF-8 BOM/CRLF입니다.

PowerShell에서 동일한 빌드:

```powershell
.\build.ps1 -Test
```

Visual Studio에서는 이 폴더의 `AlarmCsvMerge.sln`을 열고 Release/x64를 선택합니다. 중간 산출물은 다음 위치에 생성됩니다.

```text
build\v141_10.0.17134.0\x64\Release\
```

## 실행 메뉴

exe를 입력 Alarm 폴더 **밖의 쓰기 가능한 폴더**에 둡니다. 실행하면 경로 입력 전에 다음 메뉴가 나타납니다.

```text
1: Compare
2: Align
선택 [1/2]:
```

`1`은 기존 Compare, `2`는 Align입니다. 앞뒤 공백은 허용하며 다른 입력은 재질문합니다. 메뉴나 폴더 경로 입력 중 EOF이면 결과를 생성하지 않고 종료 코드 `1`로 끝납니다. 리디렉션으로 입력하는 스크립트도 첫 줄에 메뉴 번호를 전달해야 합니다.

## Compare 사용 순서

1. 실행 메뉴에서 `1`을 입력합니다.
2. `Alarm Folder Path1 (OLD/Equipment)`에 기존 장비 Alarm 폴더의 절대경로를 입력합니다.
3. `Alarm Folder Path2 (NEW/PatchSet)`에 패치 Alarm 폴더의 절대경로를 입력합니다.
4. exe 옆에 생성된 `AlarmDiff_yyyyMMdd_HHmmss.txt`를 확인합니다.
5. `AutoChange? [Y/N]`에서 N이면 보고서만 남깁니다. Y이면 실제 TreatMode 차이 예제와 Y/N별 결과를 확인합니다.
6. `StopMode가 같은데 TreatMode가 다른 경우도 EQuipment의 내용을 PatchSet에 적용할까요? [Y/N]`에 응답합니다. Y이면 StopMode 차이와 TreatMode만의 차이를 모두 적용하고, N이면 StopMode가 다른 필드만 적용합니다. 결과는 exe 옆 새 Alarm 폴더에 생성됩니다.
7. 같은 보고서의 `AUTOCHANGE: PatchSet -> 출력`에서 선택한 옵션과 적용 내역을 확인합니다.

추가 질문의 예제는 상대경로 → ID → 컬럼 순서의 첫 TreatMode 차이입니다. 대상이 없으면 `대상 0건`과 설명용 예제를 보여주고 동일하게 질문합니다. 두 질문 모두 Y/N 대소문자와 앞뒤 공백을 허용하고 잘못된 입력은 재질문합니다. 입력 종료(EOF)는 해당 질문에서 N으로 처리합니다.

한글·공백이 포함된 경로, 바깥쪽 큰따옴표, UNC 경로를 지원합니다. 경로는 현재 작업 폴더가 아닌 **exe 위치**를 기준으로 출력합니다. 동일한 이름의 보고서 또는 Alarm 폴더가 이미 있으면 일시·숫자 접미사로 새 결과를 만듭니다.

콘솔 프롬프트와 보고서 카테고리 제목은 한국어입니다. 보고서의 비교 값은 `EQuipment(Path1) :`, `PatchSet(Path2)  :`로 구분하고, 검색하기 쉬운 영어 항목명과 원문 값을 사용합니다. 보고서 인코딩은 UTF-8 BOM, 줄바꿈은 CRLF입니다.

Compare가 생성하는 CSV는 기존처럼 PatchSet의 원래 행 순서를 유지합니다. 보고서의 ID 목록은 문자열 순서이며, 숫자 기준으로 CSV를 정렬하려면 Align을 사용합니다.

## Align 사용 순서와 보존 규칙

1. 실행 메뉴에서 `2`를 입력합니다.
2. `Alarm Folder Path (Align)`에 정렬할 알람 폴더 경로 하나를 입력합니다. Compare와 동일하게 한글·공백·바깥쪽 큰따옴표·UNC 경로를 지원합니다.
3. 하위 폴더까지 CSV 형식을 검증하고 정상 파일을 각각 정렬합니다. 추가 Y/N 질문은 없습니다.
4. exe 옆의 `Align_<입력폴더명>` 폴더와 콘솔에 표시된 처리 결과를 확인합니다. 예: 입력 `D:\Equipment\Alarm` → `<exe 폴더>\Align_Alarm`.

파일명·하위 폴더 구조·빈 폴더는 유지하며, 비CSV 파일도 바이트 그대로 복사합니다. 결과 폴더가 이미 있으면 `Align_Alarm_yyyyMMdd_HHmmss`, 필요시 숫자 접미사를 붙여 새 결과를 생성합니다. 입력 원본과 이전 결과는 덮어쓰지 않습니다. 드라이브 루트처럼 마지막 폴더 이름이 없으면 `Align_Alarm`을 사용합니다.

정렬 기준:

- 첫 컬럼의 CSV 해석값에서 앞뒤 공백을 제거한 값을 정렬 키로 사용합니다. 실제 ID와 행 원문은 변경하지 않습니다.
- `0~9`만 있는 ID는 숫자값으로 비교합니다. 예: `2 → 10 → 100`. 정수형 범위를 넘는 긴 숫자도 지원합니다.
- `001`과 `1`, `000`과 `0`처럼 숫자값이 같으면 기존 상대 순서를 유지합니다. 문자 포함 ID는 숫자 ID 뒤에 대소문자를 구분하는 문자열 순으로 배치합니다.
- 각 알람 행 바로 앞의 연속된 주석·빈 줄은 해당 행과 함께 이동합니다. 헤더와 헤더 앞 주석·빈 줄은 맨 앞에, 마지막 알람 뒤의 주석·빈 줄은 맨 끝에 유지합니다.

모든 컬럼 값, 공백, 쉼표, 따옴표, 마지막 빈 필드를 포함한 **원본 행 전체**를 이동합니다. 인코딩·BOM을 보존하고 줄바꿈은 기존 물리적 행 위치의 구분자를 사용하므로, 혼합 줄바꿈과 마지막 개행 유무도 유지합니다. 이미 정렬된 파일, 데이터 행이 하나인 파일, 헤더만 있는 정상 파일은 바이트 그대로 복사합니다.

기존 알람 CSV 검증에 실패한 파일(중복·빈 ID, 컬럼 수 불일치, 잘못된 따옴표, 인코딩 오류 등)은 정렬하지 않고 원문 그대로 복사합니다. 정렬 후 재검증에서 행 내용이나 줄 경계가 달라지는 파일도 원문을 유지합니다. 예를 들어 혼합 CR/LF 사이로 빈 줄이 이동하면서 줄 경계가 합쳐지는 경우는 정렬에서 제외합니다. 정상 파일의 정렬은 계속 진행하고, 제외 파일의 상대경로와 사유를 콘솔에 표시합니다.

콘솔에는 출력 경로와 CSV 전체·정렬·미변경·제외·비CSV 복사 건수를 표시합니다. CSV가 없으면 결과 폴더 없이 정상 종료합니다. 파일을 읽지 못하거나 링크·정션 등으로 입력 목록이 불완전하면 전체 생성을 중단합니다. 임시 폴더에서 저장 후 재읽기 검증을 마친 뒤 결과를 확정하며, 출력 실패 시 이번 실행의 결과만 정리합니다.

## 비교 규칙

- 폴더 트리 전체를 조사합니다. 파일의 대응 키는 대소문자를 구분하지 않는 상대경로입니다. `A\BM.csv`와 `B\BM.csv`는 별개입니다.
- Diff Report는 아래 다섯 카테고리를 순서대로 출력합니다. 각 카테고리는 그 종류의 차이가 있는 파일만 기록하며, 차이가 없으면 제목과 `차이 없음`을 표시합니다. 공통 파일 목록과 Rules 설명문은 출력하지 않습니다.
- 비CSV 파일은 Diff 카테고리에서 제외합니다. 전체 파일 집계와 AutoChange의 비CSV 복사·실행 기록에는 기존처럼 포함합니다.
- ID는 정렬 여부와 관계없이 앞뒤 공백을 제거한 문자열로 대응합니다. `001`, `1`, `A001`, `a001`은 서로 다른 ID입니다.
- 컬럼 값은 앞뒤 공백을 무시하여 비교하지만, 대소문자와 `|` 항목 순서는 구분합니다.
- Run·Setup·Default는 CSV 해석값의 첫 번째 `:` 앞을 StopMode, 뒤 전체를 TreatMode로 나누고 각 부분의 앞뒤 공백을 무시합니다. `:`가 없으면 TreatMode는 빈 값이므로 `Ignore`와 `Ignore:`는 같습니다. `Ignore`와 `Ignore:0`은 TreatMode 차이이며 `EMGStop:4|5|6`과 `EMGStop:6|5|4`도 TreatMode 차이입니다. StopMode가 다르면 TreatMode도 달라도 StopMode 영역에만 기록합니다.
- 컬럼 차이는 파일 → ID → 컬럼 위치 순서로 기록하며, ID, 컬럼 번호·이름, 양쪽 원본 Line과 Path1·Path2 값을 표시합니다. Line은 주석과 빈 줄을 포함한 1부터 시작하는 물리적 행 번호입니다.
- 모든 컬럼은 같은 위치끼리 비교합니다. 헤더명 차이는 무시하고, 없는 컬럼과 빈 값을 구분합니다. 차이는 컬럼 번호와 표시명으로 식별하며 표시명은 Path2, Path1, Column N 순으로 선택합니다.
- 오류 파일은 다섯 카테고리 뒤의 `오류 및 비교 제외 내역`에 기록합니다. 파일 오류 때문에 비교하지 못한 항목은 `NOT COMPARED`로 표시하고 차이 건수에서 제외합니다. 입력 목록이나 CSV 비교가 불완전하면 관련 빈 카테고리에 `비교 가능한 항목에서 차이 없음 — 오류 내역 참조`를 표시합니다. 선택한 변경의 출력 인코딩 오류는 생성 단계의 AutoChange 기록에 남기며, 이미 확인한 값 차이는 보고서에 유지합니다.
- 마지막 SUMMARY에는 생략된 정상 파일까지 포함한 전체 파일·CSV·ID 집계와 StopMode·TreatMode-only·기타 컬럼 차이 건수를 기록합니다. 여러 차이 영역에 등장한 공통 ID는 파일별로 한 번만 셉니다. `Comparable IDs (common CSVs)`의 전용 ID 건수는 공통 CSV 내부의 ID 차이만 집계합니다.

## Diff Report 카테고리

| 순서 / 제목 | 출력 내용 |
|---|---|
| 1. `<알람 파일 List 차이>` | CSV만 대상으로 `[Path1 ONLY]`, `[Path2 ONLY]` 아래에 한쪽 전용 파일의 상대경로를 정렬하여 기록 |
| 2. `<파일별 StopMode 차이>` | 공통 CSV의 공통 ID에서 Run·Setup·Default의 StopMode 부분이 다른 항목 |
| 3. `<파일별 StopMode는 동일하지만 TreatMode가 다른 리스트>` | StopMode는 같고 TreatMode만 다른 항목을 `[TREATMODE]`로 기록 |
| 4. `<파일별로 Alarm ID 기준으로 한쪽에만 있는 리스트>` | 공통 CSV 내부의 각 ID 앞에 `[EQ Only(PATH1)]` 또는 `[PatchSet Only(PATH2)]`를 붙여 `ID, Name, Line` 기록 |
| 5. `<파일별 Alarm ID 기준으로 컬럼별 내용이 다른 리스트>` | 공통 CSV의 공통 ID에서 세 Mode 컬럼을 제외한 값 차이. ID별 양쪽 원본 행 전체를 먼저 한 번씩 출력하고 컬럼별 차이를 나열 |

한쪽에만 존재하는 CSV의 모든 ID를 4번에 펼쳐 쓰지 않습니다. 그런 파일은 1번에만 표시하고, 유효성 오류가 있으면 별도 진단 영역에도 기록합니다. 파일은 대소문자를 구분하지 않는 상대경로 순서, ID는 문자열 순서, 컬럼은 위치 순서로 정렬합니다. 각 파일의 빈 ID 쪽 목록은 생략합니다. 5번의 전체 행은 실제 CSV 원문이므로 쉼표·따옴표·공백을 보존하며, 한 컬럼만 달라도 출력합니다.

예시:

```text
<알람 파일 List 차이>
[Path1 ONLY]
  OldOnly.csv
[Path2 ONLY]
  NewOnly.csv

<파일별 StopMode 차이>

--- File: BM.csv ---
[STOPMODE] ID="1001" Column=8 Name="StopMode_Run" Path1 Line=2 Path2 Line=4
EQuipment(Path1) : "EMGStop:5|6"
PatchSet(Path2)  : "DisplayOnly:1"

<파일별 StopMode는 동일하지만 TreatMode가 다른 리스트>

--- File: BM.csv ---
[TREATMODE] ID="1001" Column=9 Name="StopMode_Setup" Path1 Line=2 Path2 Line=4
EQuipment(Path1) : "Ignore:0"
PatchSet(Path2)  : "Ignore:4|5|6"

<파일별로 Alarm ID 기준으로 한쪽에만 있는 리스트>

--- File: BM.csv ---
[EQ Only(PATH1)] ID="9000" Name="Old alarm" Line=3
[PatchSet Only(PATH2)] ID="3003" Name="New alarm" Line=2

<파일별 Alarm ID 기준으로 컬럼별 내용이 다른 리스트>
--- File: BM.csv ---
[OTHER COLUMN] ID:"1001"
EQuipment(Path1) : 1001,EQ alarm,WARN,Cat,Module,Sub,설명,EMGStop:5|6,Ignore:0,Default:3
PatchSet(Path2)  : 1001,Patch alarm,WARN,Cat,Module,Sub,설명,DisplayOnly:1,Ignore:4|5|6,Default:3

Column=2 Name="AlarmName" Path1 Line=2 Path2 Line=4
EQuipment(Path1) : "EQ alarm"
PatchSet(Path2)  : "Patch alarm"
```

## 지원하는 SCTC CSV 형식

CSV는 헤더명과 무관하게 다음 위치로 해석합니다.

| 항목 | 1부터 시작하는 위치 |
|---|---:|
| ID | 1 |
| StopMode_Run | 8 |
| StopMode_Setup | 9 |
| StopMode_Default | 10 |

첫 번째 비주석·비어 있지 않은 행은 헤더이며 데이터 비교와 ID 집계에서 제외합니다. 헤더는 컬럼 수와 표시명에만 사용하고 헤더명 변경은 보고하지 않습니다. 빈 이름과 중복 이름도 허용하며 `Description` 컬럼을 요구하지 않습니다. 최소 10개 컬럼이 있어야 하고 각 데이터 행의 컬럼 수는 해당 파일의 헤더와 일치해야 합니다.

쉼표로 컬럼을 구분합니다. 값 안에 쉼표가 있으면 값 전체를 큰따옴표로 감싸며, 값 안의 큰따옴표 하나는 연속된 두 개(`""`)로 표현합니다. 감싸지 않은 필드 내부에서도 `""`를 큰따옴표 하나로 해석합니다.

| CSV 필드 원문 | 해석 결과 |
|---|---|
| `abc""def` | `abc"def` |
| `"abc,def"` | `abc,def` |
| `"abc ""def"", ghi"` | `abc "def", ghi` |
| `""abc""` | `"abc"` |
| `""` | 빈 값 |
| `""""` | `"` |

완전한 큰따옴표 필드 해석을 우선하므로 `""`는 기존처럼 빈 값입니다. 완전한 감싼 필드가 아니면 쉼표까지의 감싸지 않은 필드로 해석하되, 큰따옴표는 모두 쌍을 이루어야 합니다. 단독 큰따옴표, 닫히지 않은 큰따옴표, 잘못된 닫힘 뒤 문자, 여러 물리적 줄에 걸친 필드는 오류입니다.

빈 필드와 마지막 빈 필드를 지원합니다. 비교에는 해석한 값을 사용하고 AutoChange에는 선택된 원문 필드 구간을 사용합니다. StopMode와 TreatMode가 같으면 공백·따옴표 표기만 다른 필드는 Diff나 변경 기록에 나타나지 않으며 PatchSet 원문을 유지합니다. 보고서의 개별 비교 값에 쓰는 `\"`, `\t` 같은 표시는 텍스트 보고서용 이스케이프이며 CSV 원문을 다시 쓰는 규칙이 아닙니다. 기타 컬럼 비교 앞의 전체 CSV 행에는 이 이스케이프를 적용하지 않습니다.

빈 줄과 앞쪽 공백 뒤 `#`로 시작하는 주석은 유지합니다. 헤더명으로 컬럼 이동을 추적하지 않으므로 입력 데이터는 위의 고정 위치를 따라야 합니다.
지원 인코딩은 UTF-8(BOM 유무), CP949, BOM 있는 UTF-16 LE/BE입니다. BOM이 없으면 유효한 UTF-8을 우선하고 아니면 CP949를 시도합니다. 인코딩 판정 후 바이트 왕복 검증을 수행합니다. UTF-32, BOM 없는 UTF-16, 잘못된 문자 데이터는 제외합니다.

## AutoChange와 보존 규칙

| 조건 | 결과 |
|---|---|
| 같은 상대경로의 정상 CSV, 동일 ID에서 StopMode가 다름 | 해당 Mode 필드 전체를 EQ 원문으로 적용 (TreatMode도 함께 적용) |
| 동일 ID에서 StopMode는 같고 TreatMode만 다름 | 추가 질문 Y이면 해당 Mode 필드 전체를 EQ 원문으로 적용, N이면 PatchSet 원문 유지 |
| StopMode와 TreatMode 모두 같음 | 공백·따옴표·빈 TreatMode 구분자 표기 차이가 있어도 PatchSet 원문 유지 |
| NEW에만 있는 파일·ID | NEW 그대로 유지 |
| OLD에만 있는 파일·ID | 결과에 추가하지 않음 |
| 양쪽 중 하나라도 중복 ID·잘못된 CSV 형식·컬럼 수 불일치·인코딩 문제 존재 | 해당 NEW 파일 전체를 바이트 그대로 복사 |
| 비CSV 파일 | NEW 바이트 그대로 복사 |

세 Mode 컬럼을 각각 독립적으로 판정하며 빈 StopMode도 값으로 적용합니다. 적용 보고서는 선택한 Y/N과 실제 변경한 셀만 `[VALUE CHANGED]`로 기록하고, `변경 전 PatchSet :`와 `변경 후 출력 Mode :`로 원문 필드를 구분합니다. 변경 없이 복사한 공통 파일은 개별 기록을 생략하고 전체 집계에만 포함합니다. NEW 전용 파일과 오류로 제외한 파일은 기록합니다.

PatchSet의 선택하지 않은 모든 필드, 행 순서, 헤더, 주석, 빈 줄, 인코딩, BOM, 개별 줄바꿈과 마지막 개행 여부를 보존합니다. 선택한 필드 구간만 치환합니다. 선택한 EQ 값을 출력 인코딩으로 손실 없이 표현할 수 없으면 해당 파일 전체를 변경에서 제외하고 PatchSet 바이트 그대로 복사합니다. 선택하지 않은 TreatMode의 인코딩 문제는 StopMode 변경을 막지 않습니다. 생성 집계와 종료 코드는 실제 생성 결과의 제외 건수를 사용합니다.

예: `BM.csv / ID 1001`의 EQ Run이 `EMGStop:5|6`, PatchSet Run이 `DisplayOnly:1`이면 결과 Run은 `EMGStop:5|6`입니다. Setup이 각각 `Ignore:0`, `Ignore:4|5|6`이면 추가 질문 Y에서만 `Ignore:0`을 적용합니다. Default가 같다면 PatchSet 원문을 유지합니다.

두 Y/N 입력을 기다리는 동안 원본 파일이 바뀌어도 비교 시 확보한 내용을 사용합니다. 출력은 응답 이후 선택한 옵션으로 만듭니다. 두 입력 트리를 메모리에 보관하므로 입력 전체 크기에 비례한 메모리가 필요합니다.

입력 파일은 읽기 전용으로 열며, 결과는 새 폴더에만 씁니다. 링크·정션을 만나거나 파일을 읽지 못해 목록이 불완전하면 보고서를 남기고 생성하지 않습니다. 출력은 임시 폴더에서 파일별 읽기 검증을 마친 후 확정합니다. 실패 시 이번 실행에서 만든 결과만 정리하며, 정리에 실패하면 남은 경로를 안내합니다.

파일 내용과 폴더 구조를 복제하며 NTFS 권한, 파일 시각, 숨김 속성, 대체 데이터 스트림은 복제하지 않습니다.

## 종료 코드와 검증

- `0`: Compare 또는 Align 완료. Compare의 N 선택과 Align의 CSV 없음도 정상 종료입니다.
- `1`: 메뉴·경로 입력 종료 또는 경로·읽기·쓰기 오류 등으로 실행/생성 실패.
- `2`: 작업은 끝났지만 CSV 중 자동변경 또는 정렬에서 제외되어 원문 복사한 파일이 있음. Compare 보고서 또는 Align 콘솔의 제외 사유를 확인하세요.

`AlarmCsvMerge.Tests`는 CSV·Mode 분류·인코딩·보고서·출력 보존과 Align의 숫자 정렬·주석 묶음·오류 제외·출력 실패 정리를 검증합니다. `tests/Integration.ps1`은 임시 폴더에서 실제 exe에 메뉴와 경로를 입력하여 기존 Compare 흐름 및 Align의 하위 폴더 처리·원문 보존·오류 복사·재실행·메뉴/경로 검증을 확인합니다. 원본 WPF 테스트는 변경하지 않습니다.

개발 환경에서 다른 도구로 기능 검증할 때만 명시적으로 지정합니다:

```powershell
.\build.ps1 -Toolset v143 -Sdk 10.0.26100.0 -Test
```

대체 도구 빌드는 툴셋별 build 폴더에만 남고 dist에는 복사하지 않습니다. 현재 작업 PC의 v143/SDK 10.0.26100.0에서 빌드·테스트했고, exe의 동적 의존성은 `KERNEL32.dll`만 확인했습니다. 요청에 따라 v141/SDK 10.0.17134.0을 추가 설치하지 않았으므로, 해당 조합의 최종 빌드와 장비 PC 실행 검증은 그 도구가 설치된 환경에서 수행해야 합니다.
