#include "AlarmCore.h"
#include "MergeEngine.h"
#include "WinIo.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <utility>

using namespace alarm;

namespace
{
    const std::wstring Header = L"AlarmID,AlarmName,Severity,Category,Module,SubModule,Description,Action,Recovery,Note";
    const std::wstring NamedHeader = L"ID,AlarmName,Severity,Category,Module,SubModule,Description,StopMode_Run,StopMode_Setup,StopMode_Default";

    Bytes B(const std::wstring& text, Encoding encoding = Encoding::Utf8, bool bom = false)
    {
        return EncodeText(text, encoding, bom);
    }

    std::wstring R(const std::wstring& id, const std::wstring& run = L"DisplayOnly:1",
        const std::wstring& setup = L"SetupStop:2", const std::wstring& def = L"DefaultStop:3",
        const std::wstring& description = L"description", const std::wstring& name = L"name")
    {
        return id + L"," + name + L",WARN,Cat,Module,Sub," + description + L"," + run + L"," + setup + L"," + def;
    }

    void Check(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    bool Has(const std::wstring& text, const std::wstring& part)
    {
        return text.find(part) != std::wstring::npos;
    }

    void UnsortedMerge()
    {
        auto oldBytes = B(Header + L"\r\n" + R(L"2002") + L"\r\n" +
            R(L"1001", L"EMGStop:5|6", L"SetupOld:4", L"", L"old description", L"old name") + L"\r\n" + R(L"OLD_ONLY"));
        auto newBytes = B(Header + L"\n" + R(L"1001", L"DisplayOnly:1", L"SetupNew:5", L"DefaultStop:3", L"NEW description", L"NEW name") +
            L"\n" + R(L"NEW_ONLY") + L"\n" + R(L"2002"));
        auto expected = B(Header + L"\n" + R(L"1001", L"EMGStop:5|6", L"SetupOld:4", L"", L"NEW description", L"NEW name") +
            L"\n" + R(L"NEW_ONLY") + L"\n" + R(L"2002"));
        auto beforeOld = oldBytes, beforeNew = newBytes;
        auto result = CompareCsv(&oldBytes, &newBytes);
        Check(!result.skipped && result.output == expected, "Exact requested BM/1001 merge");
        Check(result.stopCells == 3 && result.otherCells == 2 && result.differentIds == 1, "Diff counts");
        Check(result.commonIds == 2 && result.oldOnlyIds == 1 && result.newOnlyIds == 1, "Unsorted matching");
        Check(result.changes.size() == 3 && Has(result.diff, L"EMGStop:5|6") && Has(result.diff, L"OLD ONLY ID"), "Full diff");
        Check(oldBytes == beforeOld && newBytes == beforeNew, "Original buffers immutable");
    }

    void DescriptionAndLineEndings()
    {
        auto description = L"abc 'abc \"\"abc\"\", abc \"\"abc\"\" abc";
        auto oldBytes = B(Header + L"\r\n" + R(L"A", L"OLD"));
        auto newText = L"  # comment\r\n\n" + Header + L",Enabled\r" +
            R(L"A", L"NEW", L"SetupStop:2", L"DefaultStop:3", description) + L",TRUE\n# end\r\n";
        auto expected = L"  # comment\r\n\n" + Header + L",Enabled\r" +
            R(L"A", L"OLD", L"SetupStop:2", L"DefaultStop:3", description) + L",TRUE\n# end\r\n";
        auto newBytes = B(newText, Encoding::Utf8, true);
        auto result = CompareCsv(&oldBytes, &newBytes);
        Check(result.output == B(expected, Encoding::Utf8, true), "Untouched quotes, commas, extra field, mixed endings and BOM");
    }

    void WhitespaceAndIdIdentity()
    {
        auto oldBytes = B(Header + L"\n" + R(L" A001 ", L" EMG:1 ", L"\tSetup:2\t", L"") +
            L"\n" + R(L"001") + L"\n" + R(L"a001"));
        auto newBytes = B(Header + L"\n" + R(L"A001", L"EMG:1", L"Setup:2", L"  ") + L"\n" + R(L"1"));
        auto result = CompareCsv(&oldBytes, &newBytes);
        Check(result.commonIds == 1 && result.oldOnlyIds == 2 && result.newOnlyIds == 1, "IDs preserve zeroes and case");
        Check(result.stopCells == 0 && result.differentIds == 0 && result.changes.size() == 3, "Whitespace ignored in diff");
        for (const auto& change : result.changes) Check(change.whitespaceOnly, "Whitespace changes classified");
        auto expected = B(Header + L"\n" + R(L"A001", L" EMG:1 ", L"\tSetup:2\t", L"") + L"\n" + R(L"1"));
        Check(result.output == expected, "All three raw OLD StopMode fields transferred");
    }

    void HeaderMapping()
    {
        auto oldBytes = B(Header + L",First,Second\n" + R(L"A") + L",one,two");
        auto newBytes = B(NamedHeader + L",Second,First\n" + R(L"A") + L",two,one");
        auto result = CompareCsv(&oldBytes, &newBytes);
        Check(!result.skipped && result.stopCells == 0 && result.otherCells == 0, "Aliases and extra column reordering");
        Check(result.output == newBytes && Has(result.diff, L"HEADER CHANGED"), "NEW header unchanged");
    }

    void MissingVersusEmptyColumn()
    {
        auto oldBytes = B(Header + L",Removed\n" + R(L"A") + L",");
        auto newBytes = B(Header + L",Added\n" + R(L"A") + L",");
        auto result = CompareCsv(&oldBytes, &newBytes);
        Check(result.otherCells == 2 && Has(result.diff, L"<MISSING COLUMN>"), "Missing column is not empty");
        Check(Has(result.diff, L"NEW ONLY COLUMN") && Has(result.diff, L"OLD ONLY COLUMN"), "Schema changes reported");
    }

    void DuplicatesExcludeWholeFile()
    {
        auto good = B(Header + L"\n" + R(L"A", L"NEW") + L"\n" + R(L"B", L"NEW"));
        auto duplicate = B(Header + L"\n" + R(L"A", L"OLD") + L"\n" + R(L" A ", L"OTHER") + L"\n" + R(L"B", L"OLD"));
        auto result = CompareCsv(&duplicate, &good);
        Check(result.skipped && result.output == good && result.changes.empty(), "OLD duplicate excludes entire file");
        Check(Has(result.diff, L"lines 2, 3"), "Duplicate lines reported");
        result = CompareCsv(&good, &duplicate);
        Check(result.skipped && result.output == duplicate && result.changes.empty(), "NEW duplicate excludes entire file");
    }

    void InvalidStructures()
    {
        std::vector<std::wstring> invalid = {
            L"",
            L"# comment\n",
            Header + L"\nA,few,columns",
            Header + L"\n" + R(L" "),
            L"AlarmID,Name,Severity,Category,Module,Description,SubModule,Action,Recovery,Note\n" + R(L"A"),
            L"AlarmID,Name,Severity,Category,Module,SubModule,Description,Recovery,Action,Note\n" + R(L"A"),
            Header + L",Extra,Extra\n" + R(L"A") + L",x,y",
            Header + L",\n" + R(L"A") + L",",
            Header + L"\nA,name,WARN,Cat,Module,Sub,\"multi\nline\",RUN,SETUP,DEF"
        };
        auto good = B(Header + L"\n" + R(L"A"));
        for (const auto& text : invalid)
        {
            auto bytes = B(text);
            auto result = CompareCsv(&bytes, &good);
            Check(result.skipped && result.output == good && Has(result.diff, L"ERROR"), "Invalid source copies NEW unchanged");
            result = CompareCsv(&bytes, nullptr);
            Check(result.skipped && result.output.empty() && Has(result.diff, L"OLD-only file is not generated"), "Invalid OLD-only files are never copied");
        }
    }

    void Encodings()
    {
        const Encoding encodings[] = {Encoding::Utf8, Encoding::Utf8, Encoding::Cp949, Encoding::Utf16Le, Encoding::Utf16Be};
        const bool boms[] = {false, true, false, true, true};
        auto old = B(Header + L"\n" + R(L"1001", L"정지:5|6", L"", L"Default:3", L"장비 설명"));
        for (size_t i = 0; i < 5; ++i)
        {
            auto text = L"# 한글\r\n" + Header + L"\r\n" + R(L"1001", L"표시:1", L"Setup:2", L"Default:4", L"패치 설명, 쉼표");
            auto bytes = B(text, encodings[i], boms[i]);
            auto result = CompareCsv(&old, &bytes);
            auto expected = B(L"# 한글\r\n" + Header + L"\r\n" +
                R(L"1001", L"정지:5|6", L"", L"Default:3", L"패치 설명, 쉼표"), encodings[i], boms[i]);
            Check(!result.skipped && result.output == expected, "Cross-encoding merge, Korean and NEW byte format");
            Check(EncodeDocument(DecodeDocument(bytes)) == bytes, "Byte-exact encoding round-trip");
        }
    }

    void BadEncoding()
    {
        const std::vector<Bytes> bad = {
            {0xef, 0xbb, 0xbf, 0xff}, {0xff, 0xfe, 0x00}, {0xff, 0xfe, 0x00, 0xd8},
            {0xff, 0xfe, 0, 0}, {0x81}, {0x41, 0, 0x42, 0}
        };
        for (const auto& bytes : bad) Check(!ParseCsv(bytes).errors.empty(), "Invalid/unsupported encoding detected");
    }

    void LossyMergeExcluded()
    {
        auto oldBytes = B(Header + L"\n" + R(L"A", L"\xd83d\xde00"));
        auto newBytes = B(Header + L"\n" + R(L"A", L"표시"), Encoding::Cp949);
        auto result = CompareCsv(&oldBytes, &newBytes);
        Check(result.skipped && result.output == newBytes && result.changes.empty(), "Unencodable field excludes whole file");
        Check(Has(result.diff, L"AUTOCHANGE EXCLUDED"), "Encoding exclusion documented");
    }

    void FileOnlyAndEmptyData()
    {
        auto bytes = B(Header + L"\n" + R(L"A"));
        auto result = CompareCsv(nullptr, &bytes);
        Check(result.newOnlyIds == 1 && result.output == bytes && result.newOnly, "NEW-only file preserved");
        result = CompareCsv(&bytes, nullptr);
        Check(result.oldOnlyIds == 1 && result.output.empty(), "OLD-only file never generated");
        auto emptyData = B(Header + L"\r\n");
        result = CompareCsv(&emptyData, &emptyData);
        Check(!result.skipped && result.output == emptyData && result.changes.empty(), "Header-only file is valid");
    }

    void CaseAndPipeOrder()
    {
        auto oldBytes = B(Header + L"\n" + R(L"A", L"EMG:5|6", L"Setup", L"Default"));
        auto newBytes = B(Header + L"\n" + R(L"A", L"EMG:6|5", L"setup", L"default"));
        auto result = CompareCsv(&oldBytes, &newBytes);
        Check(result.stopCells == 3, "Case and pipe order remain meaningful");
    }

    void ManyUnsortedIds()
    {
        std::wstring oldText = Header, newText = Header, expected = Header;
        for (int i = 999; i >= 0; --i) oldText += L"\n" + R(std::to_wstring(i), L"OLD" + std::to_wstring(i));
        for (int i = 0; i < 1000; ++i)
        {
            newText += L"\n" + R(std::to_wstring(i), L"NEW");
            expected += L"\n" + R(std::to_wstring(i), L"OLD" + std::to_wstring(i));
        }
        auto oldBytes = B(oldText), newBytes = B(newText);
        auto result = CompareCsv(&oldBytes, &newBytes);
        Check(result.commonIds == 1000 && result.stopCells == 1000 && result.output == B(expected), "1000 reversed IDs matched");
    }

    void PathMatchingAndReportEscaping()
    {
        PathLess less;
        Check(!less(L"Sub\\BM.csv", L"sub\\bm.CSV") && !less(L"sub\\bm.CSV", L"Sub\\BM.csv"), "Windows path case");
        Check(IsWithin(L"C:\\work\\Alarm\\sub", L"c:\\WORK\\alarm"), "Input containment");
        Check(!IsWithin(L"C:\\work\\Alarm2", L"C:\\work\\Alarm"), "Directory boundary containment");
        Check(Quote(L"\t\"a\\b\"\r\n") == L"\"\\t\\\"a\\\\b\\\"\\r\\n\"", "Report values are unambiguous");
    }

    void NoChangeExactBytes()
    {
        auto bytes = B(L"# comment\n" + Header + L"\r\n" + R(L"A") + L"\r#tail", Encoding::Utf16Be, true);
        auto result = CompareCsv(&bytes, &bytes);
        Check(!result.skipped && result.output == bytes && result.changes.empty(), "Unmodified files byte-identical");
    }
    void IoFailureRollback()
    {
        const auto parent = ExecutableDirectory();
        const auto fixture = CreateStagingDirectory(parent);
        try
        {
            const auto oldDir = JoinPath(fixture, L"old");
            const auto newDir = JoinPath(fixture, L"new");
            const auto outDir = JoinPath(fixture, L"output");
            CreateDirectoryNew(oldDir);
            CreateDirectoryNew(newDir);
            CreateDirectoryNew(outDir);
            auto oldBytes = B(Header + L"\n" + R(L"A", L"OLD"));
            auto newBytes = B(Header + L"\n" + R(L"A", L"NEW"));
            WriteNewBytes(JoinPath(oldDir, L"BM.csv"), oldBytes);
            WriteNewBytes(JoinPath(newDir, L"BM.csv"), newBytes);
            auto comparison = CompareFolders(CanonicalDirectory(oldDir), CanonicalDirectory(newDir));
            Check(comparison.canGenerate, "Fixture inventory valid");
            bool failed = false;
            try { Generate(comparison, outDir, JoinPath(outDir, L"missing-report.txt")); }
            catch (const IoError&) { failed = true; }
            Check(failed, "Report append failure must fail generation");
            auto remaining = TakeSnapshot(outDir);
            Check(remaining.errors.empty() && remaining.files.empty() && remaining.directories.empty(),
                "Failed publication must remove only its own generated tree");
            Check(ReadBytes(JoinPath(oldDir, L"BM.csv")) == oldBytes &&
                ReadBytes(JoinPath(newDir, L"BM.csv")) == newBytes, "Failed generation preserves inputs");
            failed = false;
            try { Generate(comparison, oldDir, JoinPath(outDir, L"missing-report.txt")); }
            catch (const IoError&) { failed = true; }
            Check(failed, "Engine rejects output inside input independently of UI");
        }
        catch (...)
        {
            RemoveOwnedTree(fixture, parent);
            throw;
        }
        RemoveOwnedTree(fixture, parent);
    }

    void ExclusiveOutputCreation()
    {
        const auto parent = ExecutableDirectory();
        const auto fixture = CreateStagingDirectory(parent);
        try
        {
            const auto path = JoinPath(fixture, L"existing.bin");
            const Bytes original{ 0, 255, 1, 2, 3 };
            WriteNewBytes(path, original);
            bool failed = false;
            try { WriteNewBytes(path, Bytes{ 9, 9, 9 }); }
            catch (const IoError&) { failed = true; }
            Check(failed && ReadBytes(path) == original, "CREATE_NEW must preserve existing file");
            const auto firstReport = WriteUniqueReport(fixture, L"first report\r\n");
            const auto firstBytes = ReadBytes(firstReport);
            const auto secondReport = WriteUniqueReport(fixture, L"second report\r\n");
            Check(firstReport != secondReport && ReadBytes(firstReport) == firstBytes, "Report creation cannot overwrite");
            auto stage = CreateStagingDirectory(fixture);
            WriteNewBytes(JoinPath(stage, L"result.bin"), original);
            CreateDirectoryNew(JoinPath(fixture, L"Alarm"));
            WriteNewBytes(JoinPath(fixture, L"Alarm\\sentinel.bin"), original);
            const auto published = PublishDirectory(stage, fixture);
            Check(published != JoinPath(fixture, L"Alarm") && ReadBytes(JoinPath(fixture, L"Alarm\\sentinel.bin")) == original,
                "Publishing preserves an existing Alarm directory");
        }
        catch (...)
        {
            RemoveOwnedTree(fixture, parent);
            throw;
        }
        RemoveOwnedTree(fixture, parent);
    }

}

int main()
{
    const std::vector<std::pair<const char*, std::function<void()>>> tests = {
        {"UnsortedMerge", UnsortedMerge},
        {"DescriptionAndLineEndings", DescriptionAndLineEndings},
        {"WhitespaceAndIdIdentity", WhitespaceAndIdIdentity},
        {"HeaderMapping", HeaderMapping},
        {"MissingVersusEmptyColumn", MissingVersusEmptyColumn},
        {"DuplicatesExcludeWholeFile", DuplicatesExcludeWholeFile},
        {"InvalidStructures", InvalidStructures},
        {"Encodings", Encodings},
        {"BadEncoding", BadEncoding},
        {"LossyMergeExcluded", LossyMergeExcluded},
        {"FileOnlyAndEmptyData", FileOnlyAndEmptyData},
        {"CaseAndPipeOrder", CaseAndPipeOrder},
        {"ManyUnsortedIds", ManyUnsortedIds},
        {"PathMatchingAndReportEscaping", PathMatchingAndReportEscaping},
        {"NoChangeExactBytes", NoChangeExactBytes},
        {"IoFailureRollback", IoFailureRollback},
        {"ExclusiveOutputCreation", ExclusiveOutputCreation}
    };
    size_t failed = 0;
    for (const auto& test : tests)
    {
        try { test.second(); std::cout << "PASS " << test.first << "\n"; }
        catch (const std::exception& error)
        {
            ++failed;
            std::cerr << "FAIL " << test.first << ": " << error.what() << "\n";
        }
    }
    std::cout << tests.size() - failed << "/" << tests.size() << " test groups passed\n";
    return failed ? 1 : 0;
}
