#include "MergeEngine.h"

#include "AlignEngine.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#include <iostream>

namespace
{
    bool interactiveInput = false;

    void ConfigureConsole()
    {
        DWORD mode = 0;
        interactiveInput = GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode) != FALSE;
        if (interactiveInput) _setmode(_fileno(stdin), _O_U16TEXT);
        _setmode(_fileno(stdout), _O_U8TEXT);
        _setmode(_fileno(stderr), _O_U8TEXT);
    }

    bool ReadLine(std::wstring& value)
    {
        if (interactiveInput) return static_cast<bool>(std::getline(std::wcin, value));
        // Redirected input is UTF-8; useful for repeatable integration tests.
        std::string line;
        if (!std::getline(std::cin, line)) return false;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) { value.clear(); return true; }
        int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, line.data(), static_cast<int>(line.size()), nullptr, 0);
        if (!size) throw alarm::IoError(L"Redirected input must be UTF-8.");
        value.resize(size);
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, line.data(), static_cast<int>(line.size()), &value[0], size);
        return true;
    }

    bool ReadFolder(const wchar_t* prompt, const std::wstring& exeDirectory, std::wstring& folder)
    {
        for (;;)
        {
            std::wcout << prompt << std::flush;
            std::wstring input;
            if (!ReadLine(input)) return false;
            try
            {
                folder = alarm::CanonicalDirectory(input);
                if (alarm::IsWithin(exeDirectory, folder))
                    throw alarm::IoError(L"exe 폴더가 입력 Alarm 폴더 안에 있습니다. exe를 입력 폴더 밖으로 옮겨주세요.");
                return true;
            }
            catch (const std::exception& error)
            {
                std::wcerr << L"경로 오류: " << alarm::ExceptionMessage(error) << L"\n";
                if (!interactiveInput) return false;
            }
        }
    }

    bool ReadYesNo(const wchar_t* prompt)
    {
        for (;;)
        {
            std::wcout << prompt << std::flush;
            std::wstring answer;
            if (!ReadLine(answer)) return false;
            answer = alarm::Trim(answer);
            if (answer == L"Y" || answer == L"y") return true;
            if (answer == L"N" || answer == L"n") return false;
            std::wcout << L"Y 또는 N을 입력해주세요.\n";
        }
    }

    int Finish(int code)
    {
        if (interactiveInput)
        {
            std::wcout << L"\nEnter 키를 누르면 종료합니다." << std::flush;
            std::wstring ignored;
            ReadLine(ignored);
        }
        return code;
    }

    int ReadMode()
    {
        std::wcout << L"1: Compare\n2: Sort\n";
        for (;;)
        {
            std::wcout << L"선택 [1/2]: " << std::flush;
            std::wstring answer;
            if (!ReadLine(answer)) return 0;
            answer = alarm::Trim(answer);
            if (answer == L"1") return 1;
            if (answer == L"2") return 2;
            std::wcout << L"1 또는 2를 입력해주세요.\n";
        }
    }
}

int wmain()
{
    ConfigureConsole();
    try
    {
        std::wcout << L"AlarmCsvMerge - Alarm CSV Compare / Sort\n\n";
        const auto mode = ReadMode();
        if (!mode) return Finish(1);
        const auto exeDirectory = alarm::ExecutableDirectory();
        if (mode == 2)
        {
            std::wcout << L"\nSort - AlarmID 숫자 기준 오름차순 정렬\n"
                << L"결과는 exe 옆 Sort_ 폴더에 저장합니다.\n\n";
            std::wstring inputPath;
            if (!ReadFolder(L"Alarm Folder Path (Sort): ", exeDirectory, inputPath)) return Finish(1);
            std::wcout << L"\nCSV 검증 및 정렬 중...\n" << std::flush;
            const auto aligned = alarm::AlignFolder(inputPath, exeDirectory);
            if (!aligned.directory.empty()) std::wcout << L"\nSort 완료\n";
            std::wcout << aligned.summary;
            return Finish(aligned.excludedFiles || aligned.excludedRows ? 2 : 0);
        }
        std::wcout << L"\nCompare - Alarm CSV 비교 및 StopMode 적용\n"
            << L"Path1 = OLD/Equipment, Path2 = NEW/PatchSet\n"
            << L"결과는 exe 옆 새 Alarm 폴더에 저장합니다.\n\n";
        std::wstring oldPath, newPath;
        if (!ReadFolder(L"Alarm Folder Path1 (OLD/Equipment): ", exeDirectory, oldPath) ||
            !ReadFolder(L"Alarm Folder Path2 (NEW/PatchSet): ", exeDirectory, newPath))
            return Finish(1);
        std::wcout << L"\n파일 읽기 및 비교 중...\n" << std::flush;
        auto comparison = alarm::CompareFolders(oldPath, newPath);
        const auto reportPath = alarm::WriteUniqueReport(exeDirectory, comparison.report);
        std::wcout << L"\n" << comparison.summary << L"\nDiff 보고서: " << reportPath << L"\n";
        if (!comparison.canGenerate)
        {
            std::wcerr << L"입력 전체를 읽지 못해 AutoChange를 수행할 수 없습니다. 보고서를 확인해주세요.\n";
            return Finish(1);
        }
        if (!ReadYesNo(L"\nAutoChange? [Y/N]: "))
        {
            alarm::AppendReport(reportPath, L"\r\n[AUTOCHANGE NOT REQUESTED]\r\n");
            std::wcout << L"Diff 보고서 저장 완료. 결과 폴더를 생성하지 않았습니다.\n";
            return Finish(comparison.excludedFiles ? 2 : 0);
        }
        std::wcout << L"\nStopMode 동일 / TreatMode 차이 대상 " << comparison.treatCells << L"건\n";
        if (!comparison.treatModeExample.empty())
            std::wcout << comparison.treatModeExample;
        else
            std::wcout << L"설명용 예제 (실제 변경 대상 아님):\n"
                << L"EQuipment(Path1) : Ignore:0\nPatchSet(Path2)  : Ignore:4|5|6\n"
                << L"Y 선택 시 출력 Mode : Ignore:0\nN 선택 시 출력 Mode : Ignore:4|5|6\n";
        alarm::MergeOptions options;
        options.includeTreatModeDifferences = ReadYesNo(
            L"\nStopMode가 같은데 TreatMode가 다른 경우도 EQuipment의 내용을 PatchSet에 적용할까요? [Y/N]: ");
        std::wcout << L"\n새 Alarm 폴더 생성 및 검증 중...\n" << std::flush;
        auto generated = alarm::Generate(comparison, exeDirectory, reportPath, options);
        std::wcout << L"\n생성 완료\n" << generated.summary << L"보고서: " << reportPath << L"\n";
        if (generated.excludedFiles)
            std::wcout << L"일부 CSV는 오류로 자동변경에서 제외되어 Path2 그대로 복사되었습니다. 보고서를 확인해주세요.\n";
        return Finish(generated.excludedFiles ? 2 : 0);
    }
    catch (const std::exception& error)
    {
        std::wcerr << L"\n실패: " << alarm::ExceptionMessage(error) << L"\n";
        return Finish(1);
    }
}
