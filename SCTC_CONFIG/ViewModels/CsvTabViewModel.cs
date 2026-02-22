using CommunityToolkit.Mvvm.ComponentModel;

namespace SCTC_CONFIG.ViewModels;

public class CsvTabViewModel : ObservableObject
{
    public MainPanelViewModel MainPanel { get; } = new();
    public DriverPanelViewModel DriverPanel { get; } = new();
    public FunctionPanelViewModel FunctionPanel { get; } = new();
    public AlarmPanelViewModel AlarmPanel { get; } = new();
}
