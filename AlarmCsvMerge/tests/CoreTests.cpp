#include "AlarmCore.h"
#include "MergeEngine.h"
#include "AlignEngine.h"
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

    // Existing regression scenarios exercise comparison and generation together.
    struct ComparedAndMerged : PairResult
    {
        Bytes output;
        std::vector<CellChange> changes;
    };

    ComparedAndMerged CompareAndMerge(const Bytes* oldBytes, const Bytes* newBytes)
    {
        ComparedAndMerged result;
        static_cast<PairResult&>(result) = CompareCsv(oldBytes, newBytes);
        auto merged = MergeCsv(oldBytes, newBytes, { true });
        result.output = std::move(merged.output);
        result.changes = std::move(merged.changes);
        result.skipped = result.skipped || merged.skipped;
        if (result.diagnostics.empty()) result.diagnostics = merged.diagnostics;
        return result;
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
        auto result = CompareAndMerge(&oldBytes, &newBytes);
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
        auto result = CompareAndMerge(&oldBytes, &newBytes);
        Check(result.output == B(expected, Encoding::Utf8, true), "Untouched quotes, commas, extra field, mixed endings and BOM");
    }

    void WhitespaceAndIdIdentity()
    {
        auto oldBytes = B(Header + L"\n" + R(L" A001 ", L" EMG:1 ", L"\tSetup:2\t", L"") +
            L"\n" + R(L"001") + L"\n" + R(L"a001"));
        auto newBytes = B(Header + L"\n" + R(L"A001", L"EMG:1", L"Setup:2", L"  ") + L"\n" + R(L"1"));
        auto result = CompareAndMerge(&oldBytes, &newBytes);
        Check(result.commonIds == 1 && result.oldOnlyIds == 2 && result.newOnlyIds == 1, "IDs preserve zeroes and case");
        Check(result.stopCells == 0 && result.treatCells == 0 && result.differentIds == 0 &&
            result.changes.empty(), "Whitespace-only differences are never applied");
        Check(result.output == newBytes, "PatchSet raw formatting is preserved");
    }

    void HeaderMapping()
    {
        auto oldBytes = B(Header + L",First,Second\n" + R(L"A") + L",one,two");
        auto newBytes = B(NamedHeader + L",Second,First\n" + R(L"A") + L",two,one");
        auto result = CompareAndMerge(&oldBytes, &newBytes);
        Check(!result.skipped && result.stopCells == 0 && result.otherCells == 2, "Renamed/reordered headers still compare field positions");
        Check(result.output == newBytes && !Has(result.otherColumnDiff, L"HEADER CHANGED"), "Header labels are not compared");
    }

    void MissingVersusEmptyColumn()
    {
        auto oldBytes = B(Header + L",Extra,\n" + R(L"A") + L",,");
        auto newBytes = B(Header + L",Renamed\n" + R(L"A") + L",");
        auto result = CompareAndMerge(&oldBytes, &newBytes);
        Check(!result.skipped && result.otherCells == 1 && Has(result.otherColumnDiff, L"<MISSING COLUMN>"),
            "Missing position differs from empty while renamed equal fields do not differ");
        Check(Has(result.otherColumnDiff, L"Column=12") && Has(result.otherColumnDiff, L"Column 12"), "Unnamed column uses position label");
    }

    void DuplicatesExcludeWholeFile()
    {
        auto good = B(Header + L"\n" + R(L"A", L"NEW") + L"\n" + R(L"B", L"NEW"));
        auto duplicate = B(Header + L"\n" + R(L"A", L"OLD") + L"\n" + R(L" A ", L"OTHER") + L"\n" + R(L"B", L"OLD"));
        auto result = CompareAndMerge(&duplicate, &good);
        Check(result.skipped && result.output == good && result.changes.empty(), "OLD duplicate excludes entire file");
        Check(Has(result.diagnostics, L"lines 2, 3"), "Duplicate lines reported");
        result = CompareAndMerge(&good, &duplicate);
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
            auto result = CompareAndMerge(&bytes, &good);
            Check(result.skipped && result.output == good && Has(result.diagnostics, L"ERROR"), "Invalid source copies NEW unchanged");
            result = CompareAndMerge(&bytes, nullptr);
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
            auto result = CompareAndMerge(&oldBytes, &newBytes);
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
        auto result = CompareAndMerge(&oldBytes, &newBytes);
        Check(!result.skipped && result.commonIds == 1 && result.stopCells == 0 && result.otherCells == 2,
            "Quoted IDs/values decode correctly and compare by position");
        Check(result.output == newBytes && result.changes.empty(), "Quote-only differences preserve PatchSet raw fields");
        auto duplicate = B(Header + L"\n" + R(L"1001") + L"\n\"1001\",name,WARN,Cat,Module,Sub,message,RUN,SETUP,DEFAULT");
        Check(CompareAndMerge(&duplicate, &newBytes).skipped, "Quoted and unquoted equivalent IDs are duplicates");
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
            const auto diff = CompareAndMerge(&bytes, &equivalent);
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
        const auto oldBytes = B(oldText);
        const Encoding encodings[] = { Encoding::Utf8, Encoding::Utf8, Encoding::Cp949, Encoding::Utf16Le, Encoding::Utf16Be };
        const bool boms[] = { false, true, false, true, true };
        for (size_t i = 0; i < 5; ++i)
        {
            const auto newBytes = B(newText, encodings[i], boms[i]);
            const auto result = CompareAndMerge(&oldBytes, &newBytes);
            Check(!result.skipped && result.commonIds == 1 && result.stopCells == 0 && result.otherCells == 2,
                "Doubled-quote IDs match standard quoted IDs");
            Check(result.output == newBytes && result.changes.empty(),
                "Equivalent doubled-quote spellings preserve all PatchSet bytes across encodings");
            Check(ParseCsv(result.output).errors.empty(), "Merged doubled-quote fields remain valid");
        }
        const auto duplicate = B(oldText + L"\n" + R(L"A\"B"));
        const auto good = B(Header + L"\n" + R(L"A\"B"));
        const auto result = CompareAndMerge(&duplicate, &good);
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
            auto result = CompareAndMerge(&old, &bytes);
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
        auto result = CompareAndMerge(&oldBytes, &newBytes);
        Check(result.skipped && result.output == newBytes && result.changes.empty(), "Unencodable field excludes whole file");
        Check(Has(result.diagnostics, L"AUTOCHANGE EXCLUDED"), "Encoding exclusion documented");
    }

    void FileOnlyAndEmptyData()
    {
        auto bytes = B(Header + L"\n" + R(L"A"));
        auto result = CompareAndMerge(nullptr, &bytes);
        Check(result.newIds == 1 && result.newOnlyIds == 0 && result.newOnlyDiff.empty() &&
            result.output == bytes && result.newOnly, "NEW-only file preserved without ID-only details");
        result = CompareAndMerge(&bytes, nullptr);
        Check(result.oldIds == 1 && result.oldOnlyIds == 0 && result.oldOnlyDiff.empty() &&
            result.output.empty(), "OLD-only file never generated or expanded into ID-only details");
        auto emptyData = B(Header + L"\r\n");
        result = CompareAndMerge(&emptyData, &emptyData);
        Check(!result.skipped && result.output == emptyData && result.changes.empty(), "Header-only file is valid");
    }

    void CaseAndPipeOrder()
    {
        auto oldBytes = B(Header + L"\n" + R(L"A", L"EMG:5|6", L"Setup", L"Default"));
        auto newBytes = B(Header + L"\n" + R(L"A", L"EMG:6|5", L"setup", L"default"));
        auto result = CompareAndMerge(&oldBytes, &newBytes);
        Check(result.stopCells == 2 && result.treatCells == 1, "Case and pipe order remain meaningful in separate categories");
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
        auto result = CompareAndMerge(&oldBytes, &newBytes);
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
        auto result = CompareAndMerge(&bytes, &bytes);
        Check(!result.skipped && result.output == bytes && result.changes.empty(), "Unmodified files byte-identical");
    }

    void ModeClassificationAndSelection()
    {
        struct Case { const wchar_t* eq; const wchar_t* patch; ModeDifference kind; };
        const Case cases[] = {
            { L"Ignore", L"Ignore:", ModeDifference::None },
            { L" Ignore : 0 ", L"Ignore:0", ModeDifference::None },
            { L"", L":", ModeDifference::None },
            { L"Ignore", L"Ignore:0", ModeDifference::TreatMode },
            { L"Ignore:0", L"Ignore", ModeDifference::TreatMode },
            { L"EMGStop:4|5|6", L"EMGStop:6|5|4", ModeDifference::TreatMode },
            { L"Ignore:0", L"Ignore:1", ModeDifference::TreatMode },
            { L"Ignore:ABC", L"Ignore:abc", ModeDifference::TreatMode },
            { L"Ignore:0:1", L"Ignore:0:2", ModeDifference::TreatMode },
            { L"Ignore:4,5", L"Ignore:4|5", ModeDifference::TreatMode },
            { L"Ignore:0", L"ignore:0", ModeDifference::StopMode },
            { L"Ignore:0", L"EMGStop:4|5|6", ModeDifference::StopMode },
            { L"", L"Ignore", ModeDifference::StopMode }
        };
        for (const auto& entry : cases)
        {
            const auto oldBytes = B(Header + L"\n" + R(L"A", entry.eq));
            const auto newBytes = B(Header + L"\r\n" + R(L"A", entry.patch));
            const auto diff = CompareCsv(&oldBytes, &newBytes);
            Check(!diff.skipped && diff.stopCells == (entry.kind == ModeDifference::StopMode ? 1u : 0u) &&
                diff.treatCells == (entry.kind == ModeDifference::TreatMode ? 1u : 0u) &&
                diff.differentIds == (entry.kind == ModeDifference::None ? 0u : 1u),
                "Mode classification follows the first colon and semantic values");
            Check(diff.stopModeDiff.empty() == (entry.kind != ModeDifference::StopMode) &&
                diff.treatModeDiff.empty() == (entry.kind != ModeDifference::TreatMode),
                "Mode categories are mutually exclusive per cell");
            for (bool includeTreat : { false, true })
            {
                const auto merged = MergeCsv(&oldBytes, &newBytes, { includeTreat });
                const bool selected = entry.kind == ModeDifference::StopMode ||
                    (includeTreat && entry.kind == ModeDifference::TreatMode);
                const auto expected = selected ? B(Header + L"\r\n" + R(L"A", entry.eq)) : newBytes;
                Check(!merged.skipped && merged.output == expected && merged.changes.size() == (selected ? 1u : 0u),
                    "Y/N changes only selected values, preserving absent colons and raw formatting otherwise");
            }
        }
        const auto oldBytes = B(Header +
            L"\nA,eq,WARN,Cat,Module,Sub,old,\"EMGStop:4|5|6\",\"Ignore:0\",\"Same : 9 \"");
        const auto prefix = Header + L"\r\nA,patch,WARN,Cat,Module,Sub,new,";
        const auto newBytes = B(prefix + L"DisplayOnly:1,Ignore:4|5|6,Same:9");
        const auto diff = CompareCsv(&oldBytes, &newBytes);
        Check(diff.stopCells == 1 && diff.treatCells == 1 && diff.otherCells == 2 && diff.differentIds == 1,
            "One ID contributes once across all three difference categories");
        const auto no = MergeCsv(&oldBytes, &newBytes, { false });
        const auto yes = MergeCsv(&oldBytes, &newBytes, { true });
        Check(no.output == B(prefix + L"\"EMGStop:4|5|6\",Ignore:4|5|6,Same:9") && no.changes.size() == 1,
            "N copies the entire StopMode-different raw field only");
        Check(yes.output == B(prefix + L"\"EMGStop:4|5|6\",\"Ignore:0\",Same:9") && yes.changes.size() == 2 &&
            yes.changes[1].kind == ModeDifference::TreatMode,
            "Y additionally copies TreatMode differences and preserves equivalent Default formatting");
    }

    size_t Count(const std::wstring& text, const std::wstring& token)
    {
        size_t count = 0, pos = 0;
        while ((pos = text.find(token, pos)) != std::wstring::npos) { ++count; pos += token.size(); }
        return count;
    }

    void OtherColumnWholeRows()
    {
        const auto eqRow = R(L"1234", L"Ignore:0", L"S", L"D", L"memo, \"quoted\"", L"eq");
        for (bool multiple : { false, true })
        {
            const auto patchRow = R(L"1234", L"Ignore:0", L"S", L"D",
                multiple ? L"new memo" : L"memo, \"quoted\"", L"patch");
            const auto oldBytes = B(Header + L"\n" + eqRow);
            const auto newBytes = B(L"# comment\r\n" + Header + L"\r\n" + patchRow);
            const auto diff = CompareCsv(&oldBytes, &newBytes);
            const auto& report = diff.otherColumnDiff;
            const auto fullRows = L"[OTHER COLUMN] ID:\"1234\"\r\nEQuipment(Path1) : " + eqRow +
                L"\r\nPatchSet(Path2)  : " + patchRow + L"\r\n\r\n";
            Check(Count(report, L"[OTHER COLUMN]") == 1 && Count(report, fullRows) == 1 &&
                report.find(fullRows) < report.find(L"Column=2"),
                "Exact raw rows appear once before even a single column difference");
            Check(Count(report, L"Column=") == (multiple ? 2u : 1u) &&
                Has(report, L"Path1 Line=2 Path2 Line=3") &&
                Has(report, L"EQuipment(Path1) : \"eq\"\r\nPatchSet(Path2)  : \"patch\""),
                "Individual column differences retain names, lines and clear side labels");
        }
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

    void SelectedEncodingExclusionAndReport()
    {
        const auto parent = ExecutableDirectory();
        const auto fixture = CreateStagingDirectory(parent);
        try
        {
            const auto oldDir = JoinPath(fixture, L"old"), newDir = JoinPath(fixture, L"new");
            const auto outDir = JoinPath(fixture, L"out");
            CreateDirectoryNew(oldDir);
            CreateDirectoryNew(newDir);
            CreateDirectoryNew(outDir);
            const auto oldBytes = B(Header + L"\n" + R(L"A", L"Ignore:\xd83d\xde00", L"OLD", L"Same", L"장비"));
            const auto newBytes = B(Header + L"\r\n" + R(L"A", L"Ignore:0", L"NEW", L"Same", L"패치"), Encoding::Cp949);
            WriteNewBytes(JoinPath(oldDir, L"Mixed.csv"), oldBytes);
            WriteNewBytes(JoinPath(newDir, L"Mixed.csv"), newBytes);
            const auto treatOld = B(Header + L"\n" + R(L"Z", L"Ignore:1") + L"\n" + R(L"A", L"Ignore:2"));
            const auto treatNew = B(Header + L"\n" + R(L"Z", L"Ignore:0") + L"\n" + R(L"A", L"Ignore:0"));
            WriteNewBytes(JoinPath(oldDir, L"Treat.csv"), treatOld);
            WriteNewBytes(JoinPath(newDir, L"Treat.csv"), treatNew);
            const auto comparison = CompareFolders(oldDir, newDir);
            Check(comparison.canGenerate && comparison.excludedFiles == 0 && comparison.treatCells == 3,
                "Comparison is independent of output encoding and future Y/N choice");
            const auto stops = ReportSection(comparison.report, L"<파일별 StopMode 차이>",
                L"<파일별 StopMode는 동일하지만 TreatMode가 다른 리스트>");
            const auto treats = ReportSection(comparison.report, L"<파일별 StopMode는 동일하지만 TreatMode가 다른 리스트>",
                L"<파일별로 Alarm ID 기준으로 한쪽에만 있는 리스트>");
            Check(Has(stops, L"Mixed.csv") && !Has(stops, L"Treat.csv") && !Has(stops, L"[TREATMODE]") &&
                Has(treats, L"Mixed.csv") && Has(treats, L"Treat.csv") && !Has(treats, L"[STOPMODE]"),
                "Treat-only files are omitted from StopMode category");
            Check(Has(comparison.summary, L"Different common IDs=3 StopMode cells=1 TreatMode-only cells=3") &&
                Has(comparison.summary, L"StopMode=1 TreatMode-only=2") &&
                Has(comparison.treatModeExample, L"File: Mixed.csv\r\n[TREATMODE] ID=\"A\" Column=8"),
                "Summary and prompt example use stable file/ID/column order");
            for (bool includeTreat : { false, true })
            {
                const auto report = WriteUniqueReport(outDir, comparison.report);
                const auto generated = Generate(comparison, outDir, report, { includeTreat });
                const auto expected = includeTreat ? newBytes :
                    B(Header + L"\r\n" + R(L"A", L"Ignore:0", L"OLD", L"Same", L"패치"), Encoding::Cp949);
                Check(ReadBytes(JoinPath(generated.directory, L"Mixed.csv")) == expected &&
                    generated.excludedFiles == (includeTreat ? 1u : 0u),
                    "Unselected unencodable TreatMode does not block StopMode changes; selected failure excludes whole file");
                Check(ReadBytes(JoinPath(generated.directory, L"Treat.csv")) == (includeTreat ? treatOld : treatNew) &&
                    generated.changedFiles == 1 && generated.changedIds == (includeTreat ? 2u : 1u) &&
                    generated.changedCells == (includeTreat ? 2u : 1u) && generated.treatCells == (includeTreat ? 2u : 0u),
                    "Actual changes and exclusions determine generation statistics");
                const auto document = DecodeDocument(ReadBytes(report));
                std::wstring text;
                for (const auto& line : document.lines) text += line.text + line.ending;
                Check(Has(text, includeTreat ? L"TreatMode-only 적용 선택: Y" : L"TreatMode-only 적용 선택: N") &&
                    Has(text, L"변경 전 PatchSet : ") && Has(text, L"변경 후 출력 Mode : ") &&
                    !Has(text, L"\r\n  NEW:") && !Has(text, L"\r\n  OUTPUT:") &&
                    !Has(text, L"[FORMAT ONLY]") && Has(text, L"[AUTOCHANGE EXCLUDED]") == includeTreat,
                    "Generation records the selection, clear labels and only actual changes");
            }
            Check(ReadBytes(JoinPath(oldDir, L"Mixed.csv")) == oldBytes &&
                ReadBytes(JoinPath(newDir, L"Mixed.csv")) == newBytes, "Both input files remain immutable");
        }
        catch (...) { RemoveOwnedTree(fixture, parent); throw; }
        RemoveOwnedTree(fixture, parent);
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
            const auto stops = ReportSection(report, L"<파일별 StopMode 차이>", L"<파일별 StopMode는 동일하지만 TreatMode가 다른 리스트>");
            const auto treats = ReportSection(report, L"<파일별 StopMode는 동일하지만 TreatMode가 다른 리스트>",
                L"<파일별로 Alarm ID 기준으로 한쪽에만 있는 리스트>");
            Check(Has(treats, L"차이 없음"), "TreatMode category remains visible without differences");
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
            Check(Has(ids, L"[EQ Only(PATH1)] ID=\"A\" Name=\"first\" Line=3\r\n[EQ Only(PATH1)] ID=\"Z\" Name=\"last\" Line=2") &&
                Has(ids, L"[PatchSet Only(PATH2)] ID=\"B\" Name=\"second\" Line=2") && !Has(ids, L"not in ID list"),
                "IDs are sorted per side and show only ID, Name, original Line");
            Check(!Has(report, L"WHOLE_OLD") && !Has(report, L"WHOLE_NEW") && !Has(report, L"Same.csv") && !Has(report, L"Format.csv"),
                "Whole-file IDs and equivalent common files are omitted");
            Check(Has(stops, L"Path1 Line=2 Path2 Line=2") && Has(stops, L"EQuipment(Path1) : \"OLD\"") &&
                Has(stops, L"PatchSet(Path2)  : \"NEW\""),
                "Cell differences identify both source lines and values");
            Check(Has(comparison.summary, L"CSV union=8 COMMON=6 Path1_ONLY=1 Path2_ONLY=1") &&
                Has(comparison.summary, L"Comparable IDs (common CSVs) COMMON=6 Path1_ONLY=3 Path2_ONLY=2") &&
                Has(comparison.summary, L"Different files: StopMode=2 TreatMode-only=0 ID-only=2 Other columns=2"),
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
                L"<파일별 StopMode는 동일하지만 TreatMode가 다른 리스트>",
                L"<파일별로 Alarm ID 기준으로 한쪽에만 있는 리스트>", L"<파일별 Alarm ID 기준으로 컬럼별 내용이 다른 리스트>" };
            for (const auto* title : titles)
                Check(Has(comparison.report, std::wstring(title) + L"\r\n차이 없음\r\n"), "Empty categories keep their title and no-difference message");
            Check(!Has(comparison.report, L"Equal.csv") && !Has(comparison.report, L"오류 및 비교 제외"), "Clean report contains no file blocks or diagnostics");
            put(equalOld, L"Invalid.csv", L"short");
            put(equalNew, L"Invalid.csv", R(L"E"));
            comparison = CompareFolders(equalOld, equalNew);
            for (size_t i = 1; i < 5; ++i)
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
            try { Generate(comparison, outDir, JoinPath(outDir, L"missing-report.txt"), {}); }
            catch (const IoError&) { failed = true; }
            Check(failed, "Report append failure must fail generation");
            auto remaining = TakeSnapshot(outDir);
            Check(remaining.errors.empty() && remaining.files.empty() && remaining.directories.empty(),
                "Failed publication must remove only its own generated tree");
            Check(ReadBytes(JoinPath(oldDir, L"BM.csv")) == oldBytes &&
                ReadBytes(JoinPath(newDir, L"BM.csv")) == newBytes, "Failed generation preserves inputs");
            failed = false;
            try { Generate(comparison, oldDir, JoinPath(outDir, L"missing-report.txt"), {}); }
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

    void AlignNumericRows()
    {
        const std::vector<std::wstring> ids = { L"100", L" 2 ", L"10", L"001", L"1", L"000", L"0",
            L"184467440737095516160000", L"99999999999999999999", L"A2", L"A10", L"a1" };
        const size_t expectedOrder[] = { 5, 6, 3, 4, 1, 2, 0, 8, 7, 10, 9, 11 };
        std::wstring input = Header, expected = Header;
        for (const auto& id : ids) input += L"\n" + R(id, L" Ignore : 0 ", L"S", L"", L"memo, \"quoted\"");
        for (const auto index : expectedOrder)
            expected += L"\n" + R(ids[index], L" Ignore : 0 ", L"S", L"", L"memo, \"quoted\"");
        const auto bytes = B(input), before = bytes;
        const auto result = AlignCsv(bytes);
        Check(!result.skipped && result.changed && result.rows == ids.size() && result.output == B(expected),
            "Align uses numeric order, stable equal numeric keys, unbounded numbers and trailing text IDs");
        Check(bytes == before, "Align never changes source bytes");
        const auto again = AlignCsv(result.output);
        Check(!again.skipped && !again.changed && again.output == result.output, "Alignment is idempotent");
    }

    void AlignBlocksAndEncodings()
    {
        const std::vector<std::wstring> text = {
            L"# file preamble", L"", Header, L"# id 10", L"",
            L"\"10\",name,WARN,Cat,Module,Sub,\"설명, \"\"인용\"\"\", Ignore:0 ,\"Setup:2\",",
            L"  # id 2", R(L"2", L"Ignore:1", L"S", L"D", L"한글"),
            L"", L"# id 1", R(L"1", L"EMG:5|6")
        };
        const size_t expectedOrder[] = { 0, 1, 2, 8, 9, 10, 6, 7, 3, 4, 5 };
        const Encoding encodings[] = { Encoding::Utf8, Encoding::Utf8, Encoding::Cp949, Encoding::Utf16Le, Encoding::Utf16Be };
        const bool boms[] = { false, true, false, true, true };
        for (size_t e = 0; e < 5; ++e)
        {
            for (bool footer : { false, true })
            {
                for (bool finalNewline : { false, true })
                {
                    Document original;
                    original.encoding = encodings[e];
                    original.bom = boms[e];
                    for (size_t i = 0; i < text.size(); ++i)
                        original.lines.push_back({ text[i], i % 3 == 0 ? L"\r\n" : i % 3 == 1 ? L"\n" : L"\r" });
                    if (footer)
                    {
                        original.lines.push_back({ L"", L"\r\n" });
                        original.lines.push_back({ L"# footer", L"\n" });
                    }
                    if (!finalNewline) original.lines.back().ending.clear();
                    auto expected = original;
                    for (size_t i = 0; i < text.size(); ++i) expected.lines[i].text = text[expectedOrder[i]];
                    const auto bytes = EncodeDocument(original);
                    const auto result = AlignCsv(bytes);
                    Check(!result.skipped && result.changed && result.output == EncodeDocument(expected),
                        "Align moves preceding comments/blanks with raw rows, preserving prefix/footer and positional endings");
                    const auto parsed = ParseCsv(result.output);
                    Check(parsed.errors.empty() && parsed.headerLine == 2 && parsed.rows.size() == 3 &&
                        parsed.rows[0].id == L"1" && parsed.rows[1].id == L"2" && parsed.rows[2].id == L"10" &&
                        parsed.document.encoding == encodings[e] && parsed.document.bom == boms[e],
                        "Aligned CSV retains encoding, BOM, header and numeric IDs");
                    Check(parsed.document.lines[parsed.rows[2].line].text == text[5],
                        "Quoted IDs, trailing empty fields and all raw formatting remain exact");
                }
            }
        }
    }

    void AlignInvalidAndUnchangedFiles()
    {
        const std::vector<Bytes> invalid = {
            B(L""), B(L"# comment only"), B(L"few,columns"),
            B(Header + L"\nshort,row"),
            B(Header + L"\n" + R(L"2") + L"\n" + R(L" 2 ")),
            B(Header + L"\n" + R(L"")),
            B(Header + L"\n2,name,WARN,Cat,Module,Sub,\"bad quote,RUN,SETUP,DEF"),
            Bytes{ 0xef, 0xbb, 0xbf, 0xff }
        };
        for (const auto& bytes : invalid)
        {
            const auto result = AlignCsv(bytes);
            Check(result.skipped && !result.changed && result.output == bytes && !result.diagnostics.empty(),
                "Invalid CSV is retained byte-for-byte with a reason");
        }
        const std::vector<std::wstring> unchanged = {
            L"# preamble\n" + Header + L"\r\n\n# footer",
            Header + L"\n" + R(L"42"),
            Header + L"\n# two\n" + R(L"2") + L"\n# ten\n" + R(L"10") + L"\n" + R(L"100")
        };
        for (const auto& text : unchanged)
        {
            const auto bytes = B(text, Encoding::Utf8, true);
            const auto result = AlignCsv(bytes);
            Check(!result.skipped && !result.changed && result.output == bytes,
                "Header-only, single-row and sorted CSVs retain exact bytes");
        }
        // Moving the blank before ID 1 to after a CR would combine it with LF.
        const auto boundary = B(Header + L"\r" + R(L"2") + L"\n\n" + R(L"1"));
        const auto result = AlignCsv(boundary);
        Check(result.skipped && !result.changed && result.output == boundary && !result.diagnostics.empty(),
            "Alignment excludes a file if mixed line endings would consume a blank line");
    }

    void AlignFailureRollback()
    {
        const auto parent = ExecutableDirectory();
        const auto fixture = CreateStagingDirectory(parent);
        try
        {
            const auto input = JoinPath(fixture, std::wstring(250, L'x'));
            const auto output = JoinPath(fixture, L"out");
            CreateDirectoryNew(input);
            CreateDirectoryNew(output);
            const auto bytes = B(Header + L"\n" + R(L"10") + L"\n" + R(L"2"));
            WriteNewBytes(JoinPath(input, L"Alarm.csv"), bytes);
            bool failed = false;
            try { AlignFolder(input, input); }
            catch (const IoError&) { failed = true; }
            Check(failed, "Align engine rejects output inside input");
            failed = false;
            // The input basename is valid, but adding Align_ exceeds the volume's
            // 255-character component limit, failing publication after staging.
            try { AlignFolder(input, output); }
            catch (const IoError&) { failed = true; }
            Check(failed, "Align publication failure is surfaced");
            const auto remaining = TakeSnapshot(output);
            Check(remaining.errors.empty() && remaining.files.empty() && remaining.directories.empty(),
                "Failed Align removes its own staged output");
            Check(ReadBytes(JoinPath(input, L"Alarm.csv")) == bytes, "Failure preserves original input");
        }
        catch (...) { RemoveOwnedTree(fixture, parent); throw; }
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
        {"ModeClassificationAndSelection", ModeClassificationAndSelection},
        {"OtherColumnWholeRows", OtherColumnWholeRows},
        {"SelectedEncodingExclusionAndReport", SelectedEncodingExclusionAndReport},
        {"ReportCategoriesAndOmissions", ReportCategoriesAndOmissions},
        {"IoFailureRollback", IoFailureRollback},
        {"AlignNumericRows", AlignNumericRows},
        {"AlignBlocksAndEncodings", AlignBlocksAndEncodings},
        {"AlignInvalidAndUnchangedFiles", AlignInvalidAndUnchangedFiles},
        {"AlignFailureRollback", AlignFailureRollback},
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
