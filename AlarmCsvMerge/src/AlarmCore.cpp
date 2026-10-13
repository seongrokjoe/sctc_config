#include "AlarmCore.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <climits>
#include <cwctype>
#include <set>
#include <sstream>
#include <stdexcept>

namespace alarm
{
    namespace
    {
        const wchar_t* StopNames[] = { L"StopMode_Run", L"StopMode_Setup", L"StopMode_Default" };

        std::runtime_error Error(const char* message) { return std::runtime_error(message); }

        void ValidateUtf16(const std::wstring& text)
        {
            for (size_t i = 0; i < text.size(); ++i)
            {
                unsigned int c = text[i];
                if (c >= 0xd800 && c <= 0xdbff)
                {
                    if (++i >= text.size() || text[i] < 0xdc00 || text[i] > 0xdfff)
                        throw Error("Invalid UTF-16 surrogate pair");
                }
                else if (c >= 0xdc00 && c <= 0xdfff)
                    throw Error("Invalid UTF-16 low surrogate");
            }
        }

        std::wstring DecodeMulti(const Bytes& bytes, size_t offset, UINT codePage)
        {
            if (bytes.size() - offset > INT_MAX) throw Error("File exceeds decoder size limit");
            int count = static_cast<int>(bytes.size() - offset);
            if (count == 0) return {};
            auto input = reinterpret_cast<const char*>(bytes.data() + offset);
            int length = MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, input, count, nullptr, 0);
            if (!length) throw Error("Invalid byte sequence for selected encoding");
            std::wstring result(length, L'\0');
            if (!MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, input, count, &result[0], length))
                throw Error("Text decoding failed");
            return result;
        }

        std::wstring JoinDocument(const Document& document)
        {
            std::wstring result;
            for (const auto& line : document.lines)
            {
                result += line.text;
                result += line.ending;
            }
            return result;
        }

        bool ParseFields(const std::wstring& line, std::vector<Field>& fields, std::wstring& error)
        {
            size_t position = 0;
            for (;;)
            {
                Field field;
                field.start = position;
                if (position < line.size() && line[position] == L'"')
                {
                    ++position;
                    bool closed = false;
                    while (position < line.size())
                    {
                        wchar_t c = line[position++];
                        if (c != L'"') field.value += c;
                        else if (position < line.size() && line[position] == L'"')
                        {
                            field.value += L'"';
                            ++position;
                        }
                        else { closed = true; break; }
                    }
                    if (!closed)
                    {
                        error = L"unclosed quoted field (multiline fields are not supported)";
                        return false;
                    }
                    if (position < line.size() && line[position] != L',')
                    {
                        error = L"unexpected character after closing quote";
                        return false;
                    }
                }
                else
                {
                    while (position < line.size() && line[position] != L',')
                    {
                        if (line[position] == L'"')
                        {
                            error = L"quote inside an unquoted field";
                            return false;
                        }
                        field.value += line[position++];
                    }
                }
                field.length = position - field.start;
                fields.push_back(std::move(field));
                if (position == line.size()) return true;
                ++position; // A trailing comma creates one final empty field.
            }
        }

        std::wstring ColumnName(const Csv& oldCsv, const Csv& newCsv, size_t column)
        {
            if (column >= 7 && column <= 9) return StopNames[column - 7];
            if (column < newCsv.headers.size() && !newCsv.headers[column].empty()) return newCsv.headers[column];
            if (column < oldCsv.headers.size() && !oldCsv.headers[column].empty()) return oldCsv.headers[column];
            return L"Column " + std::to_wstring(column + 1);
        }

        std::wstring DescribeErrors(const wchar_t* side, const Csv& csv)
        {
            std::wstring result;
            for (const auto& error : csv.errors) result += L"[ERROR " + std::wstring(side) + L"] " + error + L"\r\n";
            return result;
        }

        void AppendOnlyRows(std::wostringstream& out, const Csv& csv, const wchar_t* side)
        {
            for (const auto& entry : csv.ids)
            {
                const Row& row = csv.rows[entry.second];
                out << L"[" << side << L" ONLY ID] ID=" << Quote(row.id)
                    << L" Line=" << row.line + 1 << L" Name=" << Quote(GetField(csv, row, 1)) << L"\r\n";
            }
        }
    }

    std::wstring Trim(const std::wstring& value)
    {
        size_t first = 0, last = value.size();
        while (first < last && std::iswspace(value[first])) ++first;
        while (last > first && std::iswspace(value[last - 1])) --last;
        return value.substr(first, last - first);
    }

    std::wstring Quote(const std::wstring& value)
    {
        std::wstring result = L"\"";
        for (wchar_t c : value)
        {
            switch (c)
            {
            case L'\\': result += L"\\\\"; break;
            case L'"': result += L"\\\""; break;
            case L'\r': result += L"\\r"; break;
            case L'\n': result += L"\\n"; break;
            case L'\t': result += L"\\t"; break;
            default:
                if (c < 0x20)
                {
                    const wchar_t* hex = L"0123456789ABCDEF";
                    result += L"\\u00";
                    result += hex[(c >> 4) & 15];
                    result += hex[c & 15];
                }
                else result += c;
            }
        }
        return result + L"\"";
    }

    Bytes EncodeText(const std::wstring& text, Encoding encoding, bool bom)
    {
        ValidateUtf16(text);
        Bytes result;
        if (encoding == Encoding::Utf16Le || encoding == Encoding::Utf16Be)
        {
            bool little = encoding == Encoding::Utf16Le;
            if (bom) result = little ? Bytes{0xff, 0xfe} : Bytes{0xfe, 0xff};
            for (wchar_t c : text)
            {
                unsigned char low = static_cast<unsigned char>(c & 255);
                unsigned char high = static_cast<unsigned char>((c >> 8) & 255);
                result.push_back(little ? low : high);
                result.push_back(little ? high : low);
            }
            return result;
        }
        if (text.size() > INT_MAX) throw Error("Text exceeds encoder size limit");
        if (encoding == Encoding::Utf8 && bom) result = {0xef, 0xbb, 0xbf};
        if (text.empty()) return result;
        UINT cp = encoding == Encoding::Utf8 ? CP_UTF8 : 949;
        DWORD flags = encoding == Encoding::Utf8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS;
        BOOL usedDefault = FALSE;
        BOOL* check = encoding == Encoding::Cp949 ? &usedDefault : nullptr;
        int length = WideCharToMultiByte(cp, flags, text.data(), static_cast<int>(text.size()),
            nullptr, 0, nullptr, check);
        if (!length || usedDefault) throw Error("Text cannot be encoded without loss");
        size_t offset = result.size();
        result.resize(offset + length);
        if (!WideCharToMultiByte(cp, flags, text.data(), static_cast<int>(text.size()),
            reinterpret_cast<char*>(result.data() + offset), length, nullptr, check) || usedDefault)
            throw Error("Text encoding failed or would lose characters");
        return result;
    }

    Document DecodeDocument(const Bytes& bytes)
    {
        Document result;
        std::wstring text;
        size_t offset = 0;
        if (bytes.size() >= 4 &&
            ((bytes[0] == 0xff && bytes[1] == 0xfe && bytes[2] == 0 && bytes[3] == 0) ||
             (bytes[0] == 0 && bytes[1] == 0 && bytes[2] == 0xfe && bytes[3] == 0xff)))
            throw Error("UTF-32 is not supported");
        if (bytes.size() >= 3 && bytes[0] == 0xef && bytes[1] == 0xbb && bytes[2] == 0xbf)
        {
            result.encoding = Encoding::Utf8;
            result.bom = true;
            offset = 3;
            text = DecodeMulti(bytes, offset, CP_UTF8);
        }
        else if (bytes.size() >= 2 &&
            ((bytes[0] == 0xff && bytes[1] == 0xfe) || (bytes[0] == 0xfe && bytes[1] == 0xff)))
        {
            result.encoding = bytes[0] == 0xff ? Encoding::Utf16Le : Encoding::Utf16Be;
            result.bom = true;
            offset = 2;
            if ((bytes.size() - offset) % 2) throw Error("Odd UTF-16 byte count");
            for (size_t i = offset; i < bytes.size(); i += 2)
            {
                unsigned int c = result.encoding == Encoding::Utf16Le
                    ? bytes[i] | (bytes[i + 1] << 8) : (bytes[i] << 8) | bytes[i + 1];
                text += static_cast<wchar_t>(c);
            }
            ValidateUtf16(text);
        }
        else
        {
            try { text = DecodeMulti(bytes, 0, CP_UTF8); }
            catch (const std::exception&)
            {
                result.encoding = Encoding::Cp949;
                text = DecodeMulti(bytes, 0, 949);
            }
        }
        if (text.find(L'\0') != std::wstring::npos) throw Error("NUL character or unsupported encoding");
        if (EncodeText(text, result.encoding, result.bom) != bytes)
            throw Error("Encoding does not round-trip exactly");
        size_t start = 0;
        for (size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] != L'\r' && text[i] != L'\n') continue;
            size_t length = text[i] == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n' ? 2 : 1;
            result.lines.push_back({ text.substr(start, i - start), text.substr(i, length) });
            i += length - 1;
            start = i + 1;
        }
        if (start < text.size()) result.lines.push_back({ text.substr(start), L"" });
        return result;
    }

    Bytes EncodeDocument(const Document& document)
    {
        return EncodeText(JoinDocument(document), document.encoding, document.bom);
    }

    std::wstring EncodingName(const Document& document)
    {
        const wchar_t* name = document.encoding == Encoding::Utf8 ? L"UTF-8" :
            document.encoding == Encoding::Utf16Le ? L"UTF-16LE" :
            document.encoding == Encoding::Utf16Be ? L"UTF-16BE" : L"CP949";
        return std::wstring(name) + (document.bom ? L" BOM" : L" no BOM");
    }

    Csv ParseCsv(const Bytes& bytes)
    {
        Csv csv;
        try { csv.document = DecodeDocument(bytes); }
        catch (const std::exception& error)
        {
            std::string message = error.what();
            csv.errors.push_back(L"Encoding: " + std::wstring(message.begin(), message.end()));
            return csv;
        }
        bool header = false;
        for (size_t i = 0; i < csv.document.lines.size(); ++i)
        {
            const auto& line = csv.document.lines[i].text;
            auto trimmed = Trim(line);
            if (trimmed.empty() || trimmed[0] == L'#') continue;
            std::vector<Field> fields;
            std::wstring fieldError;
            if (!ParseFields(line, fields, fieldError))
            {
                csv.errors.push_back(L"Line " + std::to_wstring(i + 1) + L": " + fieldError);
                if (!header) return csv;
                continue;
            }
            if (!header)
            {
                header = true;
                for (const auto& field : fields) csv.headers.push_back(Trim(field.value));
                if (csv.headers.size() < 10)
                {
                    csv.errors.push_back(L"Line " + std::to_wstring(i + 1) +
                        L": header must contain at least 10 columns");
                    return csv;
                }
                continue; // Header names are display labels only, never alarm data.
            }
            if (fields.size() != csv.headers.size())
            {
                csv.errors.push_back(L"Line " + std::to_wstring(i + 1) + L": column count mismatch (expected " +
                    std::to_wstring(csv.headers.size()) + L", actual " + std::to_wstring(fields.size()) + L")");
                continue;
            }
            Row row;
            row.line = i;
            row.fields = std::move(fields);
            row.id = Trim(row.fields[0].value);
            if (row.id.empty())
                csv.errors.push_back(L"Line " + std::to_wstring(i + 1) + L": empty ID");
            else
            {
                auto existing = csv.ids.find(row.id);
                if (existing != csv.ids.end())
                    csv.errors.push_back(L"Duplicate ID " + Quote(row.id) + L" at lines " +
                        std::to_wstring(csv.rows[existing->second].line + 1) + L", " + std::to_wstring(i + 1));
                else csv.ids.emplace(row.id, csv.rows.size());
            }
            csv.rows.push_back(std::move(row));
        }
        if (!header) csv.errors.push_back(L"Missing CSV header");
        return csv;
    }

    std::wstring GetField(const Csv&, const Row& row, size_t column)
    {
        return row.fields.at(column).value;
    }

    std::wstring GetRawField(const Csv& csv, const Row& row, size_t column)
    {
        const auto& field = row.fields.at(column);
        return csv.document.lines.at(row.line).text.substr(field.start, field.length);
    }

    PairResult CompareCsv(const Bytes* oldBytes, const Bytes* newBytes)
    {
        PairResult result;
        if (newBytes) result.output = *newBytes;
        Csv oldCsv, newCsv;
        if (oldBytes) oldCsv = ParseCsv(*oldBytes);
        if (newBytes) newCsv = ParseCsv(*newBytes);
        result.oldIds = oldCsv.ids.size();
        result.newIds = newCsv.ids.size();
        result.newOnly = !oldBytes;
        std::wostringstream out;
        if (oldBytes && oldCsv.errors.empty()) out << L"OLD encoding: " << EncodingName(oldCsv.document) << L"\r\n";
        if (newBytes && newCsv.errors.empty()) out << L"NEW encoding: " << EncodingName(newCsv.document) << L"\r\n";
        out << DescribeErrors(L"OLD", oldCsv) << DescribeErrors(L"NEW", newCsv);
        if (!oldCsv.errors.empty() || !newCsv.errors.empty())
        {
            result.skipped = true;
            out << L"[NOT COMPARED] File has validation errors; "
                << (newBytes ? L"NEW bytes will be copied unchanged.\r\n" : L"OLD-only file is not generated.\r\n");
            result.diff = out.str();
            return result;
        }
        if (!oldBytes || !newBytes)
        {
            AppendOnlyRows(out, oldBytes ? oldCsv : newCsv, oldBytes ? L"OLD" : L"NEW");
            result.oldOnlyIds = oldBytes ? oldCsv.ids.size() : 0;
            result.newOnlyIds = newBytes ? newCsv.ids.size() : 0;
            result.diff = out.str();
            return result;
        }
        const size_t columnCount = (std::max)(oldCsv.headers.size(), newCsv.headers.size());
        std::set<std::wstring> ids;
        for (const auto& p : oldCsv.ids) ids.insert(p.first);
        for (const auto& p : newCsv.ids) ids.insert(p.first);
        Document merged = newCsv.document;
        for (const auto& id : ids)
        {
            auto oldEntry = oldCsv.ids.find(id), newEntry = newCsv.ids.find(id);
            if (oldEntry == oldCsv.ids.end() || newEntry == newCsv.ids.end())
            {
                bool oldOnly = newEntry == newCsv.ids.end();
                const Csv& csv = oldOnly ? oldCsv : newCsv;
                const Row& row = csv.rows[oldOnly ? oldEntry->second : newEntry->second];
                out << (oldOnly ? L"[OLD ONLY ID]" : L"[NEW ONLY ID]") << L" ID=" << Quote(id)
                    << L" Line=" << row.line + 1 << L" Name=" << Quote(GetField(csv, row, 1)) << L"\r\n";
                if (oldOnly) ++result.oldOnlyIds; else ++result.newOnlyIds;
                continue;
            }
            ++result.commonIds;
            const Row& oldRow = oldCsv.rows[oldEntry->second];
            const Row& newRow = newCsv.rows[newEntry->second];
            bool different = false;
            for (size_t column = 1; column < columnCount; ++column)
            {
                bool hasOld = column < oldRow.fields.size(), hasNew = column < newRow.fields.size();
                auto oldValue = hasOld ? GetField(oldCsv, oldRow, column) : L"";
                auto newValue = hasNew ? GetField(newCsv, newRow, column) : L"";
                if (hasOld == hasNew && Trim(oldValue) == Trim(newValue)) continue;
                bool stop = column >= 7 && column <= 9;
                if (stop) ++result.stopCells; else ++result.otherCells;
                different = true;
                out << (stop ? L"[STOPMODE]" : L"[OTHER COLUMN]") << L" ID=" << Quote(id)
                    << L" Column=" << column + 1 << L" Name=" << Quote(ColumnName(oldCsv, newCsv, column))
                    << L" OLD line=" << oldRow.line + 1 << L" NEW line=" << newRow.line + 1 << L"\r\n  OLD: "
                    << (hasOld ? Quote(oldValue) : L"<MISSING COLUMN>") << L"\r\n  NEW: "
                    << (hasNew ? Quote(newValue) : L"<MISSING COLUMN>") << L"\r\n";
            }
            if (different) ++result.differentIds;
            for (size_t column = 7; column <= 9; ++column)
            {
                auto oldValue = GetRawField(oldCsv, oldRow, column);
                auto newValue = GetRawField(newCsv, newRow, column);
                if (oldValue == newValue) continue;
                result.changes.push_back({ id, StopNames[column - 7], newValue, oldValue,
                    oldRow.line + 1, newRow.line + 1,
                    Trim(GetField(oldCsv, oldRow, column)) == Trim(GetField(newCsv, newRow, column)) });
            }
            // Replace right to left; all untouched character spans stay exactly as NEW.
            for (size_t column = 10; column-- > 7;)
            {
                const auto& field = newRow.fields[column];
                merged.lines[newRow.line].text.replace(field.start, field.length, GetRawField(oldCsv, oldRow, column));
            }
        }
        try
        {
            result.output = result.changes.empty() ? *newBytes : EncodeDocument(merged);
            // Guard against replacements that invalidate the declared format.
            auto check = ParseCsv(result.output);
            if (!check.errors.empty()) throw Error("Merged CSV failed validation");
        }
        catch (const std::exception& error)
        {
            result.output = *newBytes;
            result.changes.clear();
            result.skipped = true;
            std::string message = error.what();
            out << L"[AUTOCHANGE EXCLUDED] " << std::wstring(message.begin(), message.end())
                << L"; NEW bytes will be copied unchanged.\r\n";
        }
        if (!result.differentIds && !result.oldOnlyIds && !result.newOnlyIds)
            out << L"[NO VALUE DIFFERENCES] (leading/trailing whitespace ignored)\r\n";
        out << L"IDs OLD=" << result.oldIds << L" NEW=" << result.newIds << L" COMMON=" << result.commonIds
            << L" OLD_ONLY=" << result.oldOnlyIds << L" NEW_ONLY=" << result.newOnlyIds
            << L" DIFFERENT=" << result.differentIds << L"; StopMode cells=" << result.stopCells
            << L"; Other cells=" << result.otherCells << L"\r\n";
        result.diff = out.str();
        return result;
    }
}
