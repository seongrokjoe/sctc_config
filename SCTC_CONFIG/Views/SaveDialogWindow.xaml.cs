using SCTC_CONFIG.ViewModels;
using System.Windows;

namespace SCTC_CONFIG.Views;

public partial class SaveDialogWindow : Window
{
    public SaveDialogWindow(SaveDialogViewModel vm)
    {
        InitializeComponent();
        DataContext = vm;
        vm.RequestClose += Close;
    }
}
