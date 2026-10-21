#include "AlignEngine.h"

#include <sstream>

namespace alarm
{
    AlignmentResult AlignFolder(const std::wstring& inputPath, const std::wstring& outputParent)
    {
        const auto input = CanonicalDirectory(inputPath);
        const auto parent = CanonicalDirectory(outputParent);
        if (IsWithin(parent, input))
            throw IoError(L"Output directory must be outside the input folder.");
        const auto snapshot = TakeSnapshot(input);
        if (!snapshot.errors.empty())
        {
            std::wstring errors = L"입력 전체를 읽지 못해 Align을 수행할 수 없습니다.\r\n";
            for (const auto& error : snapshot.errors) errors += error + L"\r\n";
            throw IoError(errors);
        }
        AlignmentResult result;
        for (const auto& entry : snapshot.files)
            if (IsCsv(entry.first)) ++result.csvFiles;
        if (!result.csvFiles)
        {
            result.summary = L"알람 CSV 파일이 없습니다. 결과 폴더를 생성하지 않았습니다.\r\n";
            return result;
        }
        auto folderName = input.substr(input.find_last_of(L'\\') + 1);
        if (folderName.empty()) folderName = L"Alarm"; // Drive root.
        std::wstring owned;
        try
        {
            owned = CreateStagingDirectory(parent);
            for (const auto& relative : snapshot.directories)
                CreateDirectoryNew(JoinPath(owned, relative));
            std::wostringstream details;
            size_t nonCsv = 0;
            for (const auto& entry : snapshot.files)
            {
                const auto& file = entry.second;
                const bool csv = IsCsv(file.relative);
                AlignCsvResult aligned;
                if (csv)
                {
                    aligned = AlignCsv(file.bytes);
                    if (aligned.skipped)
                    {
                        ++result.excludedFiles;
                        details << L"\r\n[정렬 제외 / 원문 복사] " << file.relative << L"\r\n" << aligned.diagnostics;
                    }
                    else if (aligned.changed) ++result.sortedFiles;
                    else ++result.unchangedFiles;
                }
                else ++nonCsv;
                const auto& bytes = csv ? aligned.output : file.bytes;
                const auto path = JoinPath(owned, file.relative);
                WriteNewBytes(path, bytes);
                if (ReadBytes(path) != bytes) throw IoError(L"Output read-back mismatch: " + path);
            }
            owned = PublishDirectory(owned, parent, L"Align_" + folderName);
            result.directory = owned;
            std::wostringstream summary;
            summary << L"출력 경로: " << result.directory << L"\r\n"
                << L"CSV 파일=" << result.csvFiles << L" 정렬=" << result.sortedFiles
                << L" 미변경=" << result.unchangedFiles << L" 제외(원문 복사)=" << result.excludedFiles
                << L" 비CSV 복사=" << nonCsv << L"\r\n" << details.str();
            result.summary = summary.str();
            owned.clear();
            return result;
        }
        catch (const std::exception& error)
        {
            std::wstring failure = ExceptionMessage(error);
            if (!owned.empty())
            {
                try { RemoveOwnedTree(owned, parent); }
                catch (const std::exception& cleanup)
                {
                    failure += L"\r\nIncomplete output remains at " + owned +
                        L"\r\nCleanup error: " + ExceptionMessage(cleanup);
                }
            }
            throw IoError(failure);
        }
    }
}
