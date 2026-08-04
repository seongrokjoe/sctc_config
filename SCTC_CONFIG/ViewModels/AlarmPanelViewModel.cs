using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using SCTC_CONFIG.Models;
using SCTC_CONFIG.Services;
using System.Collections.ObjectModel;
using System.Windows;

namespace SCTC_CONFIG.ViewModels;

public partial class AlarmFileItemViewModel : ObservableObject
{
    public AlarmFileModel Model { get; }

    public string FileName => Model.FileName;

    [ObservableProperty]
    private bool _isDisplayOnly;

    public AlarmFileItemViewModel(AlarmFileModel model)
    {
        Model = model;
        _isDisplayOnly = model.InitialIsDisplayOnly;
    }
}

public partial class AlarmPanelViewModel : ObservableObject
{
    private readonly AlarmStopModeDiffExportService _stopModeDiffExportService = new();

    [ObservableProperty]
    private bool _applyRun = true;

    [ObservableProperty]
    private bool _applySetup = true;

    [ObservableProperty]
    private bool _applyDefault = true;

    public ObservableCollection<AlarmFileItemViewModel> Items { get; } = new();

    [RelayCommand]
    private void ExportRunDefaultDiff()
    {
        try
        {
            var result = _stopModeDiffExportService.Export(this, DateTime.Now);
            MessageBox.Show(
                $"Export가 완료되었습니다.{Environment.NewLine}파일: {result.FilePath}{Environment.NewLine}차이 개수: {result.DifferenceCount}",
                "Export",
                MessageBoxButton.OK,
                MessageBoxImage.Information);
        }
        catch (Exception ex)
        {
            MessageBox.Show(
                $"Export 중 오류가 발생했습니다:{Environment.NewLine}{ex.Message}",
                "오류",
                MessageBoxButton.OK,
                MessageBoxImage.Error);
        }
    }
}
