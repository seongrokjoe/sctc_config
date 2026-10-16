#include "MergeEngine.h"

#include <set>
#include <sstream>

namespace alarm
{
    namespace
    {
        void AppendFileDetails(std::wostringstream& out, const std::wstring& path, const std::wstring& details)
        {
            if (!details.empty()) out << L"\r\n--- File: " << path << L" ---\r\n" << details;
        }

        void AppendCategory(std::wostringstream& out, const wchar_t* title,
            const std::wstring& details, bool incomplete)
        {
            out << L"\r\n" << title << L"\r\n";
            if (!details.empty()) out << details;
            else out << (incomplete ? L"비교 가능한 항목에서 차이 없음 — 오류 내역 참조\r\n" : L"차이 없음\r\n");
        }
    }

    Comparison CompareFolders(const std::wstring& oldPath, const std::wstring& newPath)
    {
        Comparison result;
        result.oldSnapshot = TakeSnapshot(oldPath);
        result.newSnapshot = TakeSnapshot(newPath);
        result.canGenerate = result.oldSnapshot.errors.empty() && result.newSnapshot.errors.empty();
        std::wostringstream diagnostics;
        for (const auto& error : result.oldSnapshot.errors) diagnostics << L"[FATAL Path1] " << error << L"\r\n";
        for (const auto& error : result.newSnapshot.errors) diagnostics << L"[FATAL Path2] " << error << L"\r\n";
        if (!result.canGenerate)
            diagnostics << L"[INCOMPLETE INVENTORY] File-only lists use readable files only. Generation is disabled.\r\n";

        std::set<std::wstring, PathLess> paths;
        for (const auto& p : result.oldSnapshot.files) paths.insert(p.first);
        for (const auto& p : result.newSnapshot.files) paths.insert(p.first);
        size_t oldOnly = 0, newOnly = 0, common = 0, csvFiles = 0, unvalidated = 0;
        size_t oldOnlyCsv = 0, newOnlyCsv = 0, commonCsv = 0;
        size_t oldIds = 0, newIds = 0, commonIds = 0, oldOnlyIds = 0, newOnlyIds = 0;
        size_t differentIds = 0, stopCells = 0, otherCells = 0;
        size_t stopFiles = 0, treatFiles = 0, idFiles = 0, otherFiles = 0;
        std::wostringstream oldFiles, newFiles, stopModes, treatModes, onlyIds, otherColumns;
        for (const auto& path : paths)
        {
            auto oldEntry = result.oldSnapshot.files.find(path), newEntry = result.newSnapshot.files.find(path);
            const Bytes* oldBytes = oldEntry == result.oldSnapshot.files.end() ? nullptr : &oldEntry->second.bytes;
            const Bytes* newBytes = newEntry == result.newSnapshot.files.end() ? nullptr : &newEntry->second.bytes;
            if (!oldBytes) ++newOnly;
            else if (!newBytes) ++oldOnly;
            else ++common;
            if (!IsCsv(path)) continue;
            ++csvFiles;
            if (!oldBytes) { ++newOnlyCsv; newFiles << L"  " << path << L"\r\n"; }
            else if (!newBytes) { ++oldOnlyCsv; oldFiles << L"  " << path << L"\r\n"; }
            else ++commonCsv;

            PairResult pair = CompareCsv(oldBytes, newBytes);
            AppendFileDetails(stopModes, path, pair.stopModeDiff);
            AppendFileDetails(treatModes, path, pair.treatModeDiff);
            if (result.treatModeExample.empty() && !pair.treatModeExample.empty())
                result.treatModeExample = L"File: " + path + L"\r\n" + pair.treatModeExample;
            AppendFileDetails(otherColumns, path, pair.otherColumnDiff);
            const auto idDetails = pair.oldOnlyDiff + pair.newOnlyDiff;
            AppendFileDetails(onlyIds, path, idDetails);
            AppendFileDetails(diagnostics, path, pair.diagnostics);
            if (!pair.stopModeDiff.empty()) ++stopFiles;
            if (!pair.treatModeDiff.empty()) ++treatFiles;
            if (!idDetails.empty()) ++idFiles;
            if (!pair.otherColumnDiff.empty()) ++otherFiles;
            if (pair.skipped)
            {
                ++unvalidated;
                if (newBytes) ++result.excludedFiles;
            }
            oldIds += pair.oldIds;
            newIds += pair.newIds;
            commonIds += pair.commonIds;
            oldOnlyIds += pair.oldOnlyIds;
            newOnlyIds += pair.newOnlyIds;
            differentIds += pair.differentIds;
            stopCells += pair.stopCells;
            result.treatCells += pair.treatCells;
            otherCells += pair.otherCells;
            result.csvResults.emplace(path, std::move(pair));
        }

        std::wostringstream report;
        report << L"ALARM CSV DIFF\r\n"
            << L"Compared at: " << Timestamp() << L"\r\n"
            << L"Path1 (OLD/Equipment): " << oldPath << L"\r\n"
            << L"Path2 (NEW/PatchSet): " << newPath << L"\r\n";
        std::wstring inventory;
        if (oldOnlyCsv) inventory += L"[Path1 ONLY]\r\n" + oldFiles.str();
        if (newOnlyCsv) inventory += L"[Path2 ONLY]\r\n" + newFiles.str();
        const bool incomplete = !result.canGenerate || unvalidated != 0;
        AppendCategory(report, L"<알람 파일 List 차이>", inventory, !result.canGenerate);
        AppendCategory(report, L"<파일별 StopMode 차이>", stopModes.str(), incomplete);
        AppendCategory(report, L"<파일별 StopMode는 동일하지만 TreatMode가 다른 리스트>", treatModes.str(), incomplete);
        AppendCategory(report, L"<파일별로 Alarm ID 기준으로 한쪽에만 있는 리스트>", onlyIds.str(), incomplete);
        AppendCategory(report, L"<파일별 Alarm ID 기준으로 컬럼별 내용이 다른 리스트>", otherColumns.str(), incomplete);
        if (!diagnostics.str().empty()) report << L"\r\n=== 오류 및 비교 제외 내역 ===\r\n" << diagnostics.str();

        std::wostringstream summary;
        summary << L"All files Path1=" << result.oldSnapshot.files.size() << L" Path2=" << result.newSnapshot.files.size()
            << L" COMMON=" << common << L" Path1_ONLY=" << oldOnly << L" Path2_ONLY=" << newOnly << L"\r\n"
            << L"CSV union=" << csvFiles << L" COMMON=" << commonCsv
            << L" Path1_ONLY=" << oldOnlyCsv << L" Path2_ONLY=" << newOnlyCsv << L"\r\n"
            << L"CSV validation errors=" << unvalidated
            << L" Path2 files excluded from AutoChange=" << result.excludedFiles << L"\r\n"
            << L"Parsed unique IDs (all CSVs) Path1=" << oldIds << L" Path2=" << newIds << L"\r\n"
            << L"Comparable IDs (common CSVs) COMMON=" << commonIds
            << L" Path1_ONLY=" << oldOnlyIds << L" Path2_ONLY=" << newOnlyIds << L"\r\n"
            << L"Different common IDs=" << differentIds << L" StopMode cells=" << stopCells
            << L" TreatMode-only cells=" << result.treatCells << L" Other cells=" << otherCells << L"\r\n"
            << L"Different files: StopMode=" << stopFiles << L" TreatMode-only=" << treatFiles << L" ID-only=" << idFiles
            << L" Other columns=" << otherFiles << L"\r\n";
        if (incomplete) summary << L"Counts use readable files and parsed rows; uncomparable data is excluded from differences. See diagnostics.\r\n";
        summary << L"Generation: " << (result.canGenerate ? L"available" : L"DISABLED (incomplete input)") << L"\r\n";
        result.summary = summary.str();
        report << L"\r\n=== SUMMARY ===\r\n" << result.summary;
        result.report = report.str();
        return result;
    }

    GenerationResult Generate(const Comparison& comparison, const std::wstring& outputParent,
        const std::wstring& reportPath, const MergeOptions& options)
    {
        if (!comparison.canGenerate) throw IoError(L"Generation is disabled because the input inventory is incomplete.");
        auto parent = CanonicalDirectory(outputParent);
        if (IsWithin(parent, comparison.oldSnapshot.root) || IsWithin(parent, comparison.newSnapshot.root))
            throw IoError(L"Output directory must be outside both input folders.");
        GenerationResult result;
        std::wstring owned;
        try
        {
            AppendReport(reportPath, std::wstring(L"\r\n=== AUTOCHANGE: PatchSet -> 출력 ===\r\n") +
                L"TreatMode-only 적용 선택: " + (options.includeTreatModeDifferences ? L"Y" : L"N") + L"\r\n");
            owned = CreateStagingDirectory(parent);
            for (const auto& relative : comparison.newSnapshot.directories)
                CreateDirectoryNew(JoinPath(owned, relative));
            std::wostringstream details;
            size_t unchanged = 0, copiedNewOnly = 0, nonCsv = 0;
            size_t modeCounts[3] = {};
            for (const auto& entry : comparison.newSnapshot.files)
            {
                const auto& file = entry.second;
                auto pair = comparison.csvResults.find(entry.first);
                const auto oldEntry = comparison.oldSnapshot.files.find(entry.first);
                const bool newOnly = oldEntry == comparison.oldSnapshot.files.end();
                MergeResult merged;
                if (pair != comparison.csvResults.end())
                    merged = MergeCsv(newOnly ? nullptr : &oldEntry->second.bytes, &file.bytes, options);
                const Bytes& bytes = pair == comparison.csvResults.end() ? file.bytes : merged.output;
                auto path = JoinPath(owned, file.relative);
                WriteNewBytes(path, bytes);
                if (ReadBytes(path) != bytes) throw IoError(L"Output read-back mismatch: " + path);
                if (newOnly || (pair != comparison.csvResults.end() &&
                    (merged.skipped || !merged.changes.empty())))
                    details << L"\r\nFile: " << file.relative << L"\r\n";
                if (newOnly) ++copiedNewOnly;
                if (pair == comparison.csvResults.end())
                {
                    ++nonCsv;
                    if (newOnly) details << L"[COPIED NON-CSV] unchanged bytes; NEW-only file\r\n";
                }
                else if (merged.skipped)
                {
                    ++result.excludedFiles;
                    details << L"[EXCLUDED] copied PatchSet bytes unchanged.\r\n" << merged.diagnostics;
                }
                else if (newOnly)
                    details << L"[COPIED NEW-ONLY]\r\n";
                if (bytes == file.bytes) ++unchanged;
                else ++result.changedFiles;
                if (pair != comparison.csvResults.end())
                {
                    std::set<std::wstring> changedIds;
                    for (const auto& change : merged.changes)
                    {
                        changedIds.insert(change.id);
                        ++result.changedCells;
                        if (change.kind == ModeDifference::TreatMode) ++result.treatCells;
                        size_t mode = change.column == L"StopMode_Run" ? 0 : change.column == L"StopMode_Setup" ? 1 : 2;
                        ++modeCounts[mode];
                        details << L"[VALUE CHANGED]"
                            << L" ID=" << Quote(change.id) << L" Column=" << change.column
                            << L" Kind=" << (change.kind == ModeDifference::TreatMode ? L"TreatMode-only" : L"StopMode")
                            << L" EQ source line=" << change.oldLine << L" PatchSet/output line=" << change.newLine
                            << L"\r\n변경 전 PatchSet : " << Quote(change.before)
                            << L"\r\n변경 후 출력 Mode : " << Quote(change.after) << L"\r\n";
                    }
                    result.changedIds += changedIds.size();
                }
            }
            // Every output has been read back before the complete tree becomes visible.
            owned = PublishDirectory(owned, parent);
            result.directory = owned;
            std::wostringstream summary;
            summary << L"Output: " << result.directory << L"\r\n"
                << L"Files total=" << comparison.newSnapshot.files.size() << L" Modified=" << result.changedFiles
                << L" Byte-identical=" << unchanged << L" NEW-only=" << copiedNewOnly
                << L" Non-CSV=" << nonCsv << L" Excluded=" << result.excludedFiles << L"\r\n"
                << L"Modified IDs=" << result.changedIds << L" Mode cells=" << result.changedCells
                << L" StopMode=" << result.changedCells - result.treatCells
                << L" TreatMode-only=" << result.treatCells << L"\r\n"
                << L"Changed Run=" << modeCounts[0] << L" Setup=" << modeCounts[1] << L" Default=" << modeCounts[2] << L"\r\n";
            result.summary = summary.str();
            AppendReport(reportPath, result.summary + details.str() +
                L"\r\n[AUTOCHANGE COMPLETE]\r\n");
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
                    failure += L"\r\nIncomplete output remains at " + owned + L"\r\nCleanup error: " + ExceptionMessage(cleanup);
                }
            }
            try { AppendReport(reportPath, L"\r\n[AUTOCHANGE FAILED] " + failure + L"\r\n"); }
            catch (const std::exception&) { failure += L"\r\nUnable to append failure to the report."; }
            throw IoError(failure);
        }
    }
}
