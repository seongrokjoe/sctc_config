using SCTC_CONFIG.ViewModels;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;

namespace SCTC_CONFIG.Views;

public partial class FunctionPanelView : UserControl
{
    public FunctionPanelView()
    {
        InitializeComponent();
    }

    private void ToggleButton_Click(object sender, RoutedEventArgs e)
    {
        // Intercept: prevent the ToggleButton from changing IsChecked automatically.
        // Let the ViewModel drive state via OneWay binding.
        e.Handled = true;

        if (sender is ToggleButton tb && tb.DataContext is FunctionFileItemViewModel vm)
        {
            vm.ToggleCommand.Execute(null);
        }
    }
}
