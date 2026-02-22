using SCTC_CONFIG.ViewModels;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;

namespace SCTC_CONFIG.Views;

public partial class XmlFunctionPanelView : UserControl
{
    public XmlFunctionPanelView()
    {
        InitializeComponent();
    }

    private void ToggleButton_Click(object sender, RoutedEventArgs e)
    {
        e.Handled = true;

        if (sender is ToggleButton tb && tb.DataContext is XmlFunctionFileItemViewModel vm)
        {
            vm.ToggleCommand.Execute(null);
        }
    }
}
