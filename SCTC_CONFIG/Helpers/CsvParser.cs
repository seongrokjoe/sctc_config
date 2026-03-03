using System.Collections.Generic;
using System.Text;

namespace SCTC_CONFIG.Helpers;

public static class CsvParser
{
    /// <summary>
    /// RFC 4180: 쌍따옴표로 묶인 필드 내부의 콤마는 구분자로 처리하지 않는다.
    /// </summary>
    public static string[] ParseLine(string line)
    {
        var fields = new List<string>();
        int i = 0;
        bool pendingEmpty = false;

        while (i < line.Length)
        {
            pendingEmpty = false;

            if (line[i] == '"')
            {
                var sb = new StringBuilder();
                i++; // 여는 따옴표 skip
                while (i < line.Length)
                {
                    if (line[i] == '"')
                    {
                        if (i + 1 < line.Length && line[i + 1] == '"')
                        { sb.Append('"'); i += 2; }   // escaped quote ""
                        else
                        { i++; break; }                // 닫는 따옴표
                    }
                    else { sb.Append(line[i]); i++; }
                }
                fields.Add(sb.ToString());
                if (i < line.Length && line[i] == ',') { i++; pendingEmpty = true; }
            }
            else
            {
                int start = i;
                while (i < line.Length && line[i] != ',') i++;
                fields.Add(line.Substring(start, i - start));
                if (i < line.Length) { i++; pendingEmpty = true; }
            }
        }

        // trailing comma가 있었거나 빈 라인이면 마지막 빈 필드 추가
        if (pendingEmpty || fields.Count == 0)
            fields.Add(string.Empty);

        return fields.ToArray();
    }

    /// <summary>
    /// 필드 값에 콤마·쌍따옴표·개행이 포함된 경우 자동 인용부호 처리 후 조인.
    /// </summary>
    public static string JoinLine(IList<string> fields)
    {
        var parts = new string[fields.Count];
        for (int i = 0; i < fields.Count; i++)
        {
            var f = fields[i];
            if (f.Contains(',') || f.Contains('"') || f.Contains('\n'))
                parts[i] = '"' + f.Replace("\"", "\"\"") + '"';
            else
                parts[i] = f;
        }
        return string.Join(",", parts);
    }
}
