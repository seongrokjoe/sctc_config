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
    const std::wstring Header = L"Number,Label,Level,Group,Unit,Part,Message,RunFlag,SetupFlag,DefaultFlag";
    const std::wstring NamedHeader = L"ID,AlarmName,Severity,Category,Module,SubModule,Description,StopMode_Run,StopMode_Setup,StopMode_Default";

    Bytes B(const std::wstring& text, Encoding encoding = Encoding::Utf8, bool bom = false)
    {
        return EncodeText(text, encoding, bom);
    }

    std::wstring CsvValue(const std::wstring& value)
    {
        if (value.find_first_of(L",\"\r\n") == std::wstring::npos) return value;
        std::wstring escaped = L"\"";
        for (wchar_t c : value) escaped += c == L'"' ? L"\"\"" : std::wstring(1, c);
        return escaped + L"\"";
    }

    std::wstring R(const std::wstring& id, const std::wstring& run = L"DisplayOnly:1",
        const std::wstring& setup = L"SetupStop:2", const std::wstring& def = L"DefaultStop:3",
        const std::wstring& description = L"description", const std::wstring& name = L"name")
    {
        return CsvValue(id) + L"," + CsvValue(name) + L",WARN,Cat,Module,Sub," + CsvValue(description) +
            L"," + CsvValue(run) + L"," + CsvValue(setup) + L"," + CsvValue(def);
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
        Check(result.changes.size() == 3 && Has(result.stopModeDiff, L"EMGStop:5|6") &&
            Has(result.oldOnlyDiff, L"OLD_ONLY") && Has(result.newOnlyDiff, L"NEW_ONLY"), "Categorized diff");
        Check(oldBytes == beforeOld && newBytes == beforeNew, "Original buffers immutable");
    }

    void QuotedFieldsAndLineEndings()
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
        for (const auto& change : result.changes) Check(change.formatOnly, "Whitespace changes classified");
        auto expected = B(Header + L"\n" + R(L"A001", L" EMG:1 ", L"\tSetup:2\t", L"") + L"\n" + R(L"1"));
        Check(result.output == expected, "All three raw OLD StopMode fields transferred");
    }

    void HeaderMapping()
    {
        auto oldBytes = B(Header + L",First,Second\n" + R(L"A") + L",one,two");
        auto newBytes = B(NamedHeader + L",Second,First\n" + R(L"A") + L",two,one");
        auto result = CompareCsv(&oldBytes, &newBytes);
        Check(!result.skipped && result.stopCells == 0 && result.otherCells == 2, "Renamed/reordered headers still compare field positions");
        Check(result.output == newBytes && !Has(result.otherColumnDiff, L"HEADER CHANGED"), "Header labels are not compared");
    }

    void MissingVersusEmptyColumn()
    {
        auto oldBytes = B(Header + L",Extra,\n" + R(L"A") + L",,");
        auto newBytes = B(Header + L",Renamed\n" + R(L"A") + L",");
        auto result = CompareCsv(&oldBytes, &newBytes);
        Check(!result.skipped && result.otherCells == 1 && Has(result.otherColumnDiff, L"<MISSING COLUMN>"),
            "Missing position differs from empty while renamed equal fields do not differ");
        Check(Has(result.otherColumnDiff, L"Column=12") && Has(result.otherColumnDiff, L"Column 12"), "Unnamed column uses position label");
    }

    void DuplicatesExcludeWholeFile()
    {
        auto good = B(Header + L"\n" + R(L"A", L"NEW") + L"\n" + R(L"B", L"NEW"));
        auto duplicate = B(Header + L"\n" + R(L"A", L"OLD") + L"\n" + R(L" A ", L"OTHER") + L"\n" + R(L"B", L"OLD"));
        auto result = CompareCsv(&duplicate, &good);
        Check(result.skipped && result.output == good && result.changes.empty(), "OLD duplicate excludes entire file");
        Check(Has(result.diagnostics, L"lines 2, 3"), "Duplicate lines reported");
        result = CompareCsv(&good, &duplicate);
        Check(result.skipped && result.output == duplicate && result.changes.empty(), "NEW duplicate excludes entire file");
    }

    void InvalidStructures()
    {
        std::vector<std::wstring> invalid = {
            L"",
            L"# comment\n",
            L"A,B,C\n",
            Header + L"\nA,few,columns",
            Header + L"\n" + R(L" "),
            Header + L"\n" + R(L"A") + L",extra",
            Header + L"\nA,name,WARN,Cat,Module,Sub,\"unterminated,RUN,SETUP,DEF",
            Header + L"\nA,name,WARN,Cat,Module,Sub,\"closed\"garbage,RUN,SETUP,DEF",
            Header + L"\nA,name,WARN,Cat,Module,Sub,bad\"quote,RUN,SETUP,DEF",
            Header + L"\nA,name,WARN,Cat,Module,Sub,\"multi\nline\",RUN,SETUP,DEF"
        };
        auto good = B(Header + L"\n" + R(L"A"));
        for (const auto& text : invalid)
        {
            auto bytes = B(text);
            auto result = CompareCsv(&bytes, &good);
            Check(result.skipped && result.output == good && Has(result.diagnostics, L"ERROR"), "Invalid source copies NEW unchanged");
            result = CompareCsv(&bytes, nullptr);
            Check(result.skipped && result.output.empty() && Has(result.diagnostics, L"Path1-only file is not generated"), "Invalid OLD-only files are never copied");
        }
    }

    void HeaderLabelsAreNotSchema()
    {
        const std::vector<std::wstring> headers = {
            NamedHeader,
            L"Anything,Same,Same,,,Other,Message,X,Y,Z",
            std::wstring(9, L','),
            L"\"Name, label\",B,C,D,E,F,G,H,I,J"
        };
        const auto row = R(L"A", L"OLD");
        auto oldBytes = B(Header + L"\n" + row);
        for (const auto& header : headers)
        {
            auto newBytes = B(L"# comment\n" + header + L"\r\n" + row);
            auto result = CompareCsv(&oldBytes, &newBytes);
            Check(!result.skipped && result.oldIds == 1 && result.newIds == 1 && result.commonIds == 1,
                "Header is excluded from ID matching regardless of labels");
            Check(result.differentIds == 0 && result.output == newBytes && result.stopModeDiff.empty() && result.otherColumnDiff.empty(),
                "Header-only renames do not produce differences");
        }
    }

    void QuotedValuesAndRawStopModes()
    {
        auto oldBytes = B(Header + L"\n\"1001\",\"Name, \"\"old\"\"\",WARN,Cat,Module,Sub,\"memo, text\",\"EMG:5|6\",\"\",\"D:3\"");
        auto newBytes = B(NamedHeader + L"\n1001,\"Name, \"\"new\"\"\",WARN,Cat,Module,Sub,\"memo, new\",EMG:5|6,,D:3");
        auto expected = B(NamedHeader + L"\n1001,\"Name, \"\"new\"\"\",WARN,Cat,Module,Sub,\"memo, new\",\"EMG:5|6\",\"\",\"D:3\"");
        auto result = CompareCsv(&oldBytes, &newBytes);
        Check(!result.skipped && result.commonIds == 1 && result.stopCells == 0 && result.otherCells == 2,
            "Quoted IDs/values decode correctly and compare by position");
        Check(result.output == expected && result.changes.size() == 3, "Raw StopMode quoting copied, other raw spans preserved");
        for (const auto& change : result.changes) Check(change.formatOnly, "Quote-only changes are formatting");
        auto duplicate = B(Header + L"\n" + R(L"1001") + L"\n\"1001\",name,WARN,Cat,Module,Sub,message,RUN,SETUP,DEFAULT");
        Check(CompareCsv(&duplicate, &newBytes).skipped, "Quoted and unquoted equivalent IDs are duplicates");
    }

    void DoubledQuotesInFields()
    {
        const std::vector<std::pair<std::wstring, std::wstring>> cases = {
            { L"abc\"\"def", L"abc\"def" },
            { L"\"abc,def\"", L"abc,def" },
            { L"\"abc \"\"def\"\", ghi\"", L"abc \"def\", ghi" },
            { L"\"\"abc\"\"", L"\"abc\"" },
            { L"\"\"", L"" },
            { L"\"\"\"\"", L"\"" },
            { L"\"\"abc", L"\"abc" },
            { L"abc\"\"", L"abc\"" },
            { L"abc\"\"\"\"def", L"abc\"\"def" },
            { L"", L"" }
        };
        for (const auto& entry : cases)
        {
            const auto text = Header + L"\r\nA,name,WARN,Cat,Module,Sub," + entry.first +
                L",RUN,SETUP," + entry.first;
            const auto bytes = B(text);
            const auto csv = ParseCsv(bytes);
            Check(csv.errors.empty() && csv.rows.size() == 1, "Doubled quotes accepted in middle and final fields");
            const auto& row = csv.rows.at(0);
            Check(row.fields.size() == 10 && GetField(csv, row, 6) == entry.second &&
                GetField(csv, row, 9) == entry.second, "Quotes and commas decoded without changing field boundaries");
            Check(GetRawField(csv, row, 6) == entry.first && GetRawField(csv, row, 9) == entry.first,
                "Raw quote spans retained for copying, including trailing empty values");
            const auto equivalent = B(Header + L"\n" + R(L"A", L"RUN", L"SETUP", entry.second, entry.second));
            const auto diff = CompareCsv(&bytes, &equivalent);
            Check(!diff.skipped && diff.differentIds == 0 && diff.stopModeDiff.empty() &&
                diff.otherColumnDiff.empty(), "Equivalent quoted spellings are not value differences");
        }

        const std::vector<std::wstring> invalid = {
            L"abc\"def", L"abc\"\"\"def", L"\"abc", L"\"abc\"tail", L"\"\"abc\"",
            L"\"\"\"", L"abc,def", L"\"abc\nxyz\""
        };
        for (const auto& field : invalid)
        {
            const auto bytes = B(Header + L"\nA,name,WARN,Cat,Module,Sub," + field + L",RUN,SETUP,DEF");
            Check(!ParseCsv(bytes).errors.empty(), "Invalid lone quotes, unwrapped commas and multiline fields remain errors");
        }
    }

    void DoubledQuoteIdsAndRawMerge()
    {
        const auto oldText = Header + L"\nA\"\"B,\"\"old\"\",WARN,Cat,Module,Sub,old,EMG\"\"stop,\"\"setup\"\",\"\"";
        const auto newText = L"# keep\r\n" + Header + L"\r\n" +
            R(L"A\"B", L"EMG\"stop", L"\"setup\"", L"", L"memo, \"quoted\"", L"name\"new") + L"\n# tail";
        const auto expected = L"# keep\r\n" + Header +
            L"\r\n\"A\"\"B\",\"name\"\"new\",WARN,Cat,Module,Sub,\"memo, \"\"quoted\"\"\",EMG\"\"stop,\"\"setup\"\",\"\"\n# tail";
        const auto oldBytes = B(oldText);
        const Encoding encodings[] = { Encoding::Utf8, Encoding::Utf8, Encoding::Cp949, Encoding::Utf16Le, Encoding::Utf16Be };
        const bool boms[] = { false, true, false, true, true };
        for (size_t i = 0; i < 5; ++i)
        {
            const auto newBytes = B(newText, encodings[i], boms[i]);
            const auto result = CompareCsv(&oldBytes, &newBytes);
            Check(!result.skipped && result.commonIds == 1 && result.stopCells == 0 && result.otherCells == 2,
                "Doubled-quote IDs match standard quoted IDs");
            Check(result.output == B(expected, encodings[i], boms[i]) && result.changes.size() == 3,
                "Raw StopModes copied exactly across encodings; NEW non-StopModes and line endings retained");
            for (const auto& change : result.changes) Check(change.formatOnly, "Doubled-quote spelling changes are format-only");
            Check(ParseCsv(result.output).errors.empty(), "Merged doubled-quote fields remain valid");
        }
        const auto duplicate = B(oldText + L"\n" + R(L"A\"B"));
        const auto good = B(Header + L"\n" + R(L"A\"B"));
        const auto result = CompareCsv(&duplicate, &good);
        Check(result.skipped && result.output == good && Has(result.diagnostics, L"Duplicate ID"),
            "Equivalent decoded IDs still exclude an entire duplicate file");
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
        Check(Has(result.diagnostics, L"AUTOCHANGE EXCLUDED"), "Encoding exclusion documented");
    }

    void FileOnlyAndEmptyData()
    {
        auto bytes = B(Header + L"\n" + R(L"A"));
        auto result = CompareCsv(nullptr, &bytes);
        Check(result.newIds == 1 && result.newOnlyIds == 0 && result.newOnlyDiff.empty() &&
            result.output == bytes && result.newOnly, "NEW-only file preserved without ID-only details");
        result = CompareCsv(&bytes, nullptr);
        Check(result.oldIds == 1 && result.oldOnlyIds == 0 && result.oldOnlyDiff.empty() &&
            result.output.empty(), "OLD-only file never generated or expanded into ID-only details");
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
    std::wstring ReportSection(const std::wstring& report, const std::wstring& title, const std::wstring& next)
    {
        const auto first = report.find(title);
        Check(first != std::wstring::npos, "Report category is present");
        const auto start = first + title.size();
        const auto end = report.find(next, start);
        Check(end != std::wstring::npos, "Report categories are in the requested order");
        return report.substr(start, end - start);
    }

    void ReportCategoriesAndOmissions()
    {
        const auto parent = ExecutableDirectory();
        const auto fixture = CreateStagingDirectory(parent);
        try
        {
            const auto oldDir = JoinPath(fixture, L"old"), newDir = JoinPath(fixture, L"new");
            CreateDirectoryNew(oldDir);
            CreateDirectoryNew(newDir);
            auto put = [](const std::wstring& dir, const std::wstring& name, const std::wstring& rows)
            {
                WriteNewBytes(JoinPath(dir, name), B(Header + L"\n" + rows));
            };
            put(oldDir, L"Stop.csv", R(L"A", L"OLD"));
            put(newDir, L"Stop.csv", R(L"A", L"NEW"));
            put(oldDir, L"Other.csv", R(L"A", L"RUN", L"SETUP", L"DEF", L"old"));
            put(newDir, L"Other.csv", R(L"A", L"RUN", L"SETUP", L"DEF", L"new"));
            const auto idsOld = R(L"Z", L"RUN", L"SETUP", L"DEF", L"not in ID list", L"last") + L"\n" +
                R(L"A", L"RUN", L"SETUP", L"DEF", L"not in ID list", L"first") + L"\n" + R(L"COMMON");
            const auto idsNew = R(L"B", L"RUN", L"SETUP", L"DEF", L"not in ID list", L"second") + L"\n" + R(L"COMMON");
            put(oldDir, L"Ids.csv", idsOld);
            put(newDir, L"Ids.csv", idsNew);
            put(oldDir, L"Mixed.csv", R(L"A", L"OLD", L"SETUP", L"DEF", L"old") + L"\n" + R(L"ONLY_OLD"));
            put(newDir, L"Mixed.csv", R(L"A", L"NEW", L"SETUP", L"DEF", L"new") + L"\n" + R(L"ONLY_NEW"));
            put(oldDir, L"OnlyOld.csv", R(L"WHOLE_OLD"));
            put(newDir, L"OnlyNew.CSV", R(L"WHOLE_NEW"));
            put(oldDir, L"Same.csv", R(L"SAME"));
            put(newDir, L"Same.csv", R(L"SAME"));
            put(oldDir, L"Format.csv", R(L"A", L" RUN ", L"SETUP", L"DEF"));
            put(newDir, L"Format.csv", R(L"A", L"RUN", L"SETUP", L"DEF"));
            WriteNewBytes(JoinPath(newDir, L"payload.bin"), Bytes{ 0, 255 });
            auto comparison = CompareFolders(oldDir, newDir);
            const auto& report = comparison.report;
            const auto inventory = ReportSection(report, L"<알람 파일 List 차이>", L"<파일별 StopMode 차이>");
            const auto stops = ReportSection(report, L"<파일별 StopMode 차이>", L"<파일별로 Alarm ID 기준으로 한쪽에만 있는 리스트>");
            const auto ids = ReportSection(report, L"<파일별로 Alarm ID 기준으로 한쪽에만 있는 리스트>", L"<파일별 Alarm ID 기준으로 컬럼별 내용이 다른 리스트>");
            const auto columns = ReportSection(report, L"<파일별 Alarm ID 기준으로 컬럼별 내용이 다른 리스트>", L"=== SUMMARY ===");
            Check(Has(inventory, L"[Path1 ONLY]\r\n  OnlyOld.csv") && Has(inventory, L"[Path2 ONLY]\r\n  OnlyNew.CSV") &&
                !Has(inventory, L"Stop.csv") && !Has(report, L"payload.bin"), "Only CSV file inventory is displayed");
            Check(Has(stops, L"Stop.csv") && Has(stops, L"Mixed.csv") && !Has(stops, L"Other.csv") && !Has(stops, L"Ids.csv"),
                "StopMode category has only files with StopMode differences");
            Check(Has(columns, L"Other.csv") && Has(columns, L"Mixed.csv") && !Has(columns, L"Stop.csv") && !Has(columns, L"Ids.csv") &&
                !Has(columns, L"[STOPMODE]"), "Other-column category excludes StopModes");
            Check(Has(ids, L"Ids.csv") && Has(ids, L"Mixed.csv") && !Has(ids, L"Stop.csv") && !Has(ids, L"Other.csv") &&
                !Has(ids, L"OnlyOld.csv") && !Has(ids, L"OnlyNew.CSV"), "ID differences use common files only");
            Check(Has(ids, L"[Path1 ONLY ID]\r\nID=\"A\" Name=\"first\" Line=3\r\nID=\"Z\" Name=\"last\" Line=2") &&
                Has(ids, L"[Path2 ONLY ID]\r\nID=\"B\" Name=\"second\" Line=2") && !Has(ids, L"not in ID list"),
                "IDs are sorted per side and show only ID, Name, original Line");
            Check(!Has(report, L"WHOLE_OLD") && !Has(report, L"WHOLE_NEW") && !Has(report, L"Same.csv") && !Has(report, L"Format.csv"),
                "Whole-file IDs and equivalent common files are omitted");
            Check(Has(stops, L"Path1 Line=2 Path2 Line=2") && Has(stops, L"Path1: \"OLD\"") && Has(stops, L"Path2: \"NEW\""),
                "Cell differences identify both source lines and values");
            Check(Has(comparison.summary, L"CSV union=8 COMMON=6 Path1_ONLY=1 Path2_ONLY=1") &&
                Has(comparison.summary, L"Comparable IDs (common CSVs) COMMON=6 Path1_ONLY=3 Path2_ONLY=2") &&
                Has(comparison.summary, L"Different files: StopMode=2 ID-only=2 Other columns=2"),
                "Summary includes omitted files and counts only displayed ID differences");

            put(oldDir, L"Invalid.csv", R(L"D") + L"\n" + R(L"D"));
            put(newDir, L"Invalid.csv", R(L"D"));
            comparison = CompareFolders(oldDir, newDir);
            const auto details = ReportSection(comparison.report, L"<파일별 Alarm ID 기준으로 컬럼별 내용이 다른 리스트>", L"=== 오류 및 비교 제외 내역 ===");
            const auto errors = ReportSection(comparison.report, L"=== 오류 및 비교 제외 내역 ===", L"=== SUMMARY ===");
            Check(!Has(details, L"Invalid.csv") && Has(errors, L"Invalid.csv") && Has(errors, L"[ERROR Path1]") &&
                Has(errors, L"Duplicate ID") && comparison.excludedFiles == 1, "Invalid CSVs are diagnosed separately");

            const auto equalOld = JoinPath(fixture, L"equal-old"), equalNew = JoinPath(fixture, L"equal-new");
            CreateDirectoryNew(equalOld);
            CreateDirectoryNew(equalNew);
            put(equalOld, L"Equal.csv", R(L"E"));
            put(equalNew, L"Equal.csv", R(L"E"));
            comparison = CompareFolders(equalOld, equalNew);
            const wchar_t* titles[] = { L"<알람 파일 List 차이>", L"<파일별 StopMode 차이>",
                L"<파일별로 Alarm ID 기준으로 한쪽에만 있는 리스트>", L"<파일별 Alarm ID 기준으로 컬럼별 내용이 다른 리스트>" };
            for (const auto* title : titles)
                Check(Has(comparison.report, std::wstring(title) + L"\r\n차이 없음\r\n"), "Empty categories keep their title and no-difference message");
            Check(!Has(comparison.report, L"Equal.csv") && !Has(comparison.report, L"오류 및 비교 제외"), "Clean report contains no file blocks or diagnostics");
            put(equalOld, L"Invalid.csv", L"short");
            put(equalNew, L"Invalid.csv", R(L"E"));
            comparison = CompareFolders(equalOld, equalNew);
            for (size_t i = 1; i < 4; ++i)
                Check(Has(comparison.report, std::wstring(titles[i]) + L"\r\n비교 가능한 항목에서 차이 없음 — 오류 내역 참조\r\n"),
                    "Uncomparable data must not look like a fully equal comparison");
            Check(Has(comparison.report, std::wstring(titles[0]) + L"\r\n차이 없음\r\n"), "Parse failures do not invalidate a complete file inventory");
        }
        catch (...)
        {
            RemoveOwnedTree(fixture, parent);
            throw;
        }
        RemoveOwnedTree(fixture, parent);
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
            Check(!Has(comparison.report, L"Rules:") && !Has(comparison.report, L"[COMMON FILE]") &&
                Has(comparison.report, L"<알람 파일 List 차이>\r\n차이 없음"), "Compact report with no file-only differences");
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
        {"QuotedFieldsAndLineEndings", QuotedFieldsAndLineEndings},
        {"WhitespaceAndIdIdentity", WhitespaceAndIdIdentity},
        {"HeaderMapping", HeaderMapping},
        {"HeaderLabelsAreNotSchema", HeaderLabelsAreNotSchema},
        {"QuotedValuesAndRawStopModes", QuotedValuesAndRawStopModes},
        {"MissingVersusEmptyColumn", MissingVersusEmptyColumn},
        {"DuplicatesExcludeWholeFile", DuplicatesExcludeWholeFile},
        {"InvalidStructures", InvalidStructures},
        {"DoubledQuotesInFields", DoubledQuotesInFields},
        {"DoubledQuoteIdsAndRawMerge", DoubledQuoteIdsAndRawMerge},
        {"Encodings", Encodings},
        {"BadEncoding", BadEncoding},
        {"LossyMergeExcluded", LossyMergeExcluded},
        {"FileOnlyAndEmptyData", FileOnlyAndEmptyData},
        {"CaseAndPipeOrder", CaseAndPipeOrder},
        {"ManyUnsortedIds", ManyUnsortedIds},
        {"PathMatchingAndReportEscaping", PathMatchingAndReportEscaping},
        {"NoChangeExactBytes", NoChangeExactBytes},
        {"ReportCategoriesAndOmissions", ReportCategoriesAndOmissions},
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
