using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;

namespace SCTC_CONFIG.ViewModels;

public partial class SaveDialogViewModel : ObservableObject
{
    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(IsTextBoxEnabled))]
    private bool _noBackup;

    [ObservableProperty]
    private string _backupFolderName = string.Empty;

    public bool IsTextBoxEnabled => !NoBackup;

    public bool? DialogResult { get; private set; }

    public event Action? RequestClose;

    [RelayCommand]
    private void Save()
    {
        DialogResult = true;
        RequestClose?.Invoke();
    }

    [RelayCommand]
    private void Cancel()
    {
        DialogResult = false;
        RequestClose?.Invoke();
    }
}
