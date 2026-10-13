#include "MergeEngine.h"

#include <set>
#include <sstream>

namespace alarm
{
    Comparison CompareFolders(const std::wstring& oldPath, const std::wstring& newPath)
    {
        Comparison result;
        result.oldSnapshot = TakeSnapshot(oldPath);
        result.newSnapshot = TakeSnapshot(newPath);
        result.canGenerate = result.oldSnapshot.errors.empty() && result.newSnapshot.errors.empty();
        std::wostringstream report;
        report << L"ALARM CSV DIFF\r\n"
            << L"Compared at: " << Timestamp() << L"\r\n"
            << L"Path1 (OLD/Equipment): " << oldPath << L"\r\n"
            << L"Path2 (NEW/PatchSet): " << newPath << L"\r\n\r\n";
        for (const auto& error : result.oldSnapshot.errors) report << L"[FATAL OLD] " << error << L"\r\n";
        for (const auto& error : result.newSnapshot.errors) report << L"[FATAL NEW] " << error << L"\r\n";
        if (!result.canGenerate)
            report << L"[INCOMPLETE INVENTORY] File-only lists below use readable files only. Generation is disabled.\r\n\r\n";
        std::set<std::wstring, PathLess> paths;
        for (const auto& p : result.oldSnapshot.files) paths.insert(p.first);
        for (const auto& p : result.newSnapshot.files) paths.insert(p.first);
        size_t oldOnly = 0, newOnly = 0, common = 0, csvFiles = 0, unvalidated = 0;
        size_t oldIds = 0, newIds = 0, commonIds = 0, oldOnlyIds = 0, newOnlyIds = 0;
        size_t differentIds = 0, stopCells = 0, otherCells = 0;
        report << L"=== FILE INVENTORY ===\r\n";
        for (const auto& path : paths)
        {
            bool hasOld = result.oldSnapshot.files.count(path) != 0;
            bool hasNew = result.newSnapshot.files.count(path) != 0;
            if (!hasOld) { ++newOnly; report << L"[Path2 ONLY] " << path << L"\r\n"; }
            else if (!hasNew) { ++oldOnly; report << L"[Path1 ONLY] " << path << L"\r\n"; }
            else ++common;
        }
        if (!oldOnly && !newOnly && result.canGenerate) report << L"한쪽에만 있는 파일 없음\r\n";
        report << L"\r\n=== CSV DETAILS ===\r\n";
        for (const auto& path : paths)
        {
            if (!IsCsv(path)) continue;
            ++csvFiles;
            auto oldEntry = result.oldSnapshot.files.find(path), newEntry = result.newSnapshot.files.find(path);
            const Bytes* oldBytes = oldEntry == result.oldSnapshot.files.end() ? nullptr : &oldEntry->second.bytes;
            const Bytes* newBytes = newEntry == result.newSnapshot.files.end() ? nullptr : &newEntry->second.bytes;
            PairResult pair = CompareCsv(oldBytes, newBytes);
            report << L"\r\n--- File: " << path << L" ---\r\n" << pair.diff;
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
            otherCells += pair.otherCells;
            result.csvResults.emplace(path, std::move(pair));
        }
        std::wostringstream summary;
        summary << L"Files OLD=" << result.oldSnapshot.files.size() << L" NEW=" << result.newSnapshot.files.size()
            << L" COMMON=" << common << L" OLD_ONLY=" << oldOnly << L" NEW_ONLY=" << newOnly << L"\r\n"
            << L"CSV union=" << csvFiles << L" CSV errors/exclusions=" << unvalidated
            << L" NEW files excluded from AutoChange=" << result.excludedFiles << L"\r\n"
            << L"Parsed unique IDs OLD=" << oldIds << L" NEW=" << newIds
            << L"; comparable COMMON=" << commonIds << L" OLD_ONLY=" << oldOnlyIds << L" NEW_ONLY=" << newOnlyIds << L"\r\n"
            << L"Different common IDs=" << differentIds << L" StopMode cells=" << stopCells
            << L" Other cells=" << otherCells << L"\r\n";
        if (unvalidated) summary << L"Counts exclude uncomparable rows/files; see ERROR/NOT COMPARED details.\r\n";
        summary << L"Generation: " << (result.canGenerate ? L"available" : L"DISABLED (incomplete input)") << L"\r\n";
        result.summary = summary.str();
        report << L"\r\n=== SUMMARY ===\r\n" << result.summary;
        result.report = report.str();
        return result;
    }

    GenerationResult Generate(const Comparison& comparison, const std::wstring& outputParent,
        const std::wstring& reportPath)
    {
        if (!comparison.canGenerate) throw IoError(L"Generation is disabled because the input inventory is incomplete.");
        auto parent = CanonicalDirectory(outputParent);
        if (IsWithin(parent, comparison.oldSnapshot.root) || IsWithin(parent, comparison.newSnapshot.root))
            throw IoError(L"Output directory must be outside both input folders.");
        GenerationResult result;
        std::wstring owned;
        try
        {
            owned = CreateStagingDirectory(parent);
            for (const auto& relative : comparison.newSnapshot.directories)
                CreateDirectoryNew(JoinPath(owned, relative));
            std::wostringstream details;
            size_t unchanged = 0, copiedNewOnly = 0, nonCsv = 0, skipped = 0;
            size_t modeCounts[3] = {};
            for (const auto& entry : comparison.newSnapshot.files)
            {
                const auto& file = entry.second;
                auto pair = comparison.csvResults.find(entry.first);
                const Bytes& bytes = pair == comparison.csvResults.end() ? file.bytes : pair->second.output;
                auto path = JoinPath(owned, file.relative);
                WriteNewBytes(path, bytes);
                if (ReadBytes(path) != bytes) throw IoError(L"Output read-back mismatch: " + path);
                details << L"\r\nFile: " << file.relative << L"\r\n";
                bool newOnly = comparison.oldSnapshot.files.count(entry.first) == 0;
                if (newOnly) ++copiedNewOnly;
                if (pair == comparison.csvResults.end())
                {
                    ++nonCsv;
                    details << L"[COPIED NON-CSV] unchanged bytes" << (newOnly ? L"; NEW-only file" : L"") << L"\r\n";
                }
                else if (pair->second.skipped)
                {
                    ++skipped;
                    details << L"[EXCLUDED] copied NEW bytes; see CSV error details above.\r\n";
                }
                else if (pair->second.changes.empty())
                    details << (newOnly ? L"[COPIED NEW-ONLY]" : L"[UNCHANGED]") << L"\r\n";
                if (bytes == file.bytes) ++unchanged;
                else ++result.changedFiles;
                if (pair != comparison.csvResults.end())
                {
                    std::set<std::wstring> changedIds;
                    for (const auto& change : pair->second.changes)
                    {
                        changedIds.insert(change.id);
                        ++result.changedCells;
                        if (change.formatOnly) ++result.formatCells;
                        size_t mode = change.column == L"StopMode_Run" ? 0 : change.column == L"StopMode_Setup" ? 1 : 2;
                        ++modeCounts[mode];
                        details << (change.formatOnly ? L"[FORMAT ONLY]" : L"[VALUE CHANGED]")
                            << L" ID=" << Quote(change.id) << L" Column=" << change.column
                            << L" OLD source line=" << change.oldLine << L" NEW/output line=" << change.newLine
                            << L"\r\n  NEW:    " << Quote(change.before) << L"\r\n  OUTPUT: " << Quote(change.after) << L"\r\n";
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
                << L" Non-CSV=" << nonCsv << L" Excluded=" << skipped << L"\r\n"
                << L"Modified IDs=" << result.changedIds << L" StopMode cells=" << result.changedCells
                << L" Value changes=" << result.changedCells - result.formatCells
                << L" Format-only=" << result.formatCells << L"\r\n"
                << L"Changed Run=" << modeCounts[0] << L" Setup=" << modeCounts[1] << L" Default=" << modeCounts[2] << L"\r\n";
            result.summary = summary.str();
            AppendReport(reportPath, L"\r\n=== AUTOCHANGE: NEW -> OUTPUT ===\r\n" + result.summary + details.str() +
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
