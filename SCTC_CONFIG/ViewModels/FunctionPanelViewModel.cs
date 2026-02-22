using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using SCTC_CONFIG.Models;
using System.Collections.ObjectModel;
using System.Windows;

namespace SCTC_CONFIG.ViewModels;

public partial class FunctionFileItemViewModel : ObservableObject
{
    public FunctionFileModel Model { get; }

    public string FileName => Model.FileName;

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(WarningVisible))]
    private bool _isInconsistent;

    [ObservableProperty]
    [NotifyPropertyChangedFor(nameof(WarningVisible))]
    private bool _warningConfirmed;

    [ObservableProperty]
    private bool _isSimulation;

    [ObservableProperty]
    private bool _hasBeenToggled;

    public bool WarningVisible => IsInconsistent && !WarningConfirmed;

    [RelayCommand]
    private void Toggle()
    {
        if (IsInconsistent && !WarningConfirmed)
        {
            var result = MessageBox.Show(
                "이 파일의 시뮬레이션 설정이 통일되지 않았습니다.\n변경하시겠습니까?",
                "경고",
                MessageBoxButton.YesNo,
                MessageBoxImage.Warning);

            if (result == MessageBoxResult.Yes)
            {
                WarningConfirmed = true;
                IsSimulation = !IsSimulation;
                HasBeenToggled = true;
            }
            // No: do nothing, keep current state
        }
        else
        {
            IsSimulation = !IsSimulation;
            HasBeenToggled = true;
        }
    }

    public FunctionFileItemViewModel(FunctionFileModel model)
    {
        Model = model;
        _isInconsistent = model.IsInconsistent;
        _isSimulation = model.InitialIsSimulation;
        _warningConfirmed = false;
        _hasBeenToggled = false;
    }
}

public class FunctionPanelViewModel : ObservableObject
{
    public ObservableCollection<FunctionFileItemViewModel> Items { get; } = new();
}
