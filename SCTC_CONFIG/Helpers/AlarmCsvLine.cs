using System.Text;

namespace SCTC_CONFIG.Helpers;

public readonly record struct AlarmCsvField(int Start, int Length);

public sealed class AlarmCsvLine
{
    public const int DescriptionColumnIndex = 6;
    public const int DisplayOnlyActionColumnIndex = 7;
    public const int DisplayOnlyRecoveryColumnIndex = 8;
    public const int DisplayOnlyNoteColumnIndex = 9;

    private readonly string _line;
    private readonly AlarmCsvField[] _fields;

    private AlarmCsvLine(string line, AlarmCsvField[] fields)
    {
        _line = line;
        _fields = fields;
    }

    public int ColumnCount => _fields.Length;

    public string GetField(int index)
    {
        var field = _fields[index];
        return _line.Substring(field.Start, field.Length);
    }

    public string ReplaceDisplayOnlyFields(string value)
    {
        var replacements = new Dictionary<int, string>
        {
            [DisplayOnlyActionColumnIndex] = value,
            [DisplayOnlyRecoveryColumnIndex] = value,
            [DisplayOnlyNoteColumnIndex] = value
        };

        var sb = new StringBuilder(_line.Length + (value.Length * 3));
        for (int i = 0; i < _fields.Length; i++)
        {
            if (i > 0)
                sb.Append(',');

            if (replacements.TryGetValue(i, out string? replacement))
            {
                sb.Append(replacement);
                continue;
            }

            var field = _fields[i];
            sb.Append(_line, field.Start, field.Length);
        }

        return sb.ToString();
    }

    public static bool TryParse(string line, int headerColumnCount, out AlarmCsvLine parsedLine, out string errorMessage)
    {
        parsedLine = null!;
        errorMessage = string.Empty;

        if (headerColumnCount <= DisplayOnlyNoteColumnIndex)
        {
            errorMessage = $"Alarm 헤더 컬럼 수가 부족합니다. HeaderColumnCount={headerColumnCount}";
            return false;
        }

        int prefixCommaCount = DescriptionColumnIndex;
        int suffixCommaCount = headerColumnCount - DisplayOnlyActionColumnIndex;

        var leftCommas = new int[prefixCommaCount];
        var rightCommas = new int[suffixCommaCount];

        if (!TryFindLeftCommas(line, leftCommas))
        {
            errorMessage = $"Alarm 행의 앞쪽 고정 컬럼 경계를 찾지 못했습니다. 필요한 콤마 수={prefixCommaCount}";
            return false;
        }

        if (!TryFindRightCommas(line, rightCommas))
        {
            errorMessage = $"Alarm 행의 뒤쪽 컬럼 경계를 찾지 못했습니다. 필요한 콤마 수={suffixCommaCount}";
            return false;
        }

        if (leftCommas[^1] >= rightCommas[0])
        {
            errorMessage = "Alarm 행의 Description 컬럼 경계가 겹칩니다.";
            return false;
        }

        var fields = new AlarmCsvField[headerColumnCount];
        int fieldStart = 0;

        for (int i = 0; i < DescriptionColumnIndex; i++)
        {
            int commaIndex = leftCommas[i];
            fields[i] = new AlarmCsvField(fieldStart, commaIndex - fieldStart);
            fieldStart = commaIndex + 1;
        }

        int descriptionEnd = rightCommas[0];
        fields[DescriptionColumnIndex] = new AlarmCsvField(fieldStart, descriptionEnd - fieldStart);

        for (int i = 0; i < rightCommas.Length; i++)
        {
            int columnIndex = DisplayOnlyActionColumnIndex + i;
            int start = rightCommas[i] + 1;
            int end = i + 1 < rightCommas.Length ? rightCommas[i + 1] : line.Length;
            fields[columnIndex] = new AlarmCsvField(start, end - start);
        }

        parsedLine = new AlarmCsvLine(line, fields);
        return true;
    }

    private static bool TryFindLeftCommas(string line, int[] leftCommas)
    {
        int found = 0;
        for (int i = 0; i < line.Length && found < leftCommas.Length; i++)
        {
            if (line[i] != ',')
                continue;

            leftCommas[found++] = i;
        }

        return found == leftCommas.Length;
    }

    private static bool TryFindRightCommas(string line, int[] rightCommas)
    {
        int found = rightCommas.Length - 1;
        for (int i = line.Length - 1; i >= 0 && found >= 0; i--)
        {
            if (line[i] != ',')
                continue;

            rightCommas[found--] = i;
        }

        return found < 0;
    }
}
