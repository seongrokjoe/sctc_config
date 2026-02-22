using System.Globalization;
using System.Windows.Data;

namespace SCTC_CONFIG.Converters;

public class BoolToToggleTextConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        bool isTrue = value is bool b && b;
        if (parameter is string s && s.Equals("lower", StringComparison.OrdinalIgnoreCase))
            return isTrue ? "true" : "false";
        return isTrue ? "TRUE" : "FALSE";
    }

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        => throw new NotImplementedException();
}
