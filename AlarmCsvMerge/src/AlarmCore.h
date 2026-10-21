#pragma once

#include <map>
#include <string>
#include <vector>

namespace alarm
{
    using Bytes = std::vector<unsigned char>;

    enum class Encoding { Utf8, Utf16Le, Utf16Be, Cp949 };

    struct Line
    {
        std::wstring text;
        std::wstring ending;
    };

    struct Document
    {
        Encoding encoding = Encoding::Utf8;
        bool bom = false;
        std::vector<Line> lines;
    };

    struct Field
    {
        size_t start = 0;
        size_t length = 0;
        std::wstring value;
    };

    struct Row
    {
        size_t line = 0;
        std::wstring id;
        std::vector<Field> fields;
    };

    struct Csv
    {
        Document document;
        std::vector<std::wstring> headers;
        size_t headerLine = 0;
        std::vector<Row> rows;
        std::map<std::wstring, size_t> ids;
        std::vector<std::wstring> errors;
    };

    std::wstring Trim(const std::wstring& value);
    std::wstring Quote(const std::wstring& value);
    Bytes EncodeText(const std::wstring& text, Encoding encoding, bool bom);
    Document DecodeDocument(const Bytes& bytes);
    Bytes EncodeDocument(const Document& document);
    std::wstring EncodingName(const Document& document);
    Csv ParseCsv(const Bytes& bytes);
    std::wstring GetField(const Csv& csv, const Row& row, size_t column);

    std::wstring GetRawField(const Csv& csv, const Row& row, size_t column);

    enum class ModeDifference { None, StopMode, TreatMode };

    struct MergeOptions
    {
        bool includeTreatModeDifferences = false;
    };

    struct CellChange
    {
        std::wstring id;
        std::wstring column;
        std::wstring before;
        std::wstring after;
        size_t oldLine = 0;
        size_t newLine = 0;
        ModeDifference kind = ModeDifference::None;
    };

    struct PairResult
    {
        std::wstring stopModeDiff;
        std::wstring treatModeDiff;
        std::wstring treatModeExample;
        std::wstring oldOnlyDiff;
        std::wstring newOnlyDiff;
        std::wstring otherColumnDiff;
        std::wstring diagnostics;
        size_t oldIds = 0;
        size_t newIds = 0;
        size_t commonIds = 0;
        // ID-only differences are counted only within common files.
        size_t oldOnlyIds = 0;
        size_t newOnlyIds = 0;
        size_t stopCells = 0;
        size_t treatCells = 0;
        size_t otherCells = 0;
        size_t differentIds = 0;
        bool skipped = false;
        bool newOnly = false;
    };

    // Null pointers represent absent files. All inputs remain immutable.
    PairResult CompareCsv(const Bytes* oldBytes, const Bytes* newBytes);

    struct MergeResult
    {
        Bytes output;
        std::vector<CellChange> changes;
        std::wstring diagnostics;
        bool skipped = false;
    };

    MergeResult MergeCsv(const Bytes* oldBytes, const Bytes* newBytes, const MergeOptions& options);

    struct SortRowIssue
    {
        size_t line = 0; // Zero-based source line.
        std::wstring id;
        bool idReadable = false;
        std::wstring reason;
        std::wstring text;
    };

    struct AlignCsvResult
    {
        Bytes output;
        std::wstring diagnostics;
        std::vector<SortRowIssue> excludedRows;
        size_t rows = 0;
        bool changed = false;
        bool skipped = false;
    };

    AlignCsvResult AlignCsv(const Bytes& bytes);
}
