using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using SCTC_CONFIG.Services;
using System.IO;
using System.Windows;

namespace SCTC_CONFIG.ViewModels;

public partial class MainViewModel : ObservableObject
{
    private readonly FileLoadService _csvLoadService = new();
    private readonly FileSaveService _csvSaveService = new();
    private readonly XmlFileLoadService _xmlLoadService = new();
    private readonly XmlFileSaveService _xmlSaveService = new();
    private readonly FolderBrowserService _browserService = new();

    [ObservableProperty]
    [NotifyCanExecuteChangedFor(nameof(LoadCommand))]
    private string _folderPath = @"D:\K12_SCTC\SCTCApplication\dat\clean_csv";

    [ObservableProperty]
    [NotifyCanExecuteChangedFor(nameof(SaveCommand))]
    private bool _isCsvLoaded;

    [ObservableProperty]
    [NotifyCanExecuteChangedFor(nameof(SaveCommand))]
    private bool _isXmlLoaded;

    [ObservableProperty]
    [NotifyCanExecuteChangedFor(nameof(SaveCommand))]
    private int _selectedTabIndex;

    public CsvTabViewModel CsvTab { get; } = new();
    public XmlTabViewModel XmlTab { get; } = new();

    [RelayCommand]
    private void Browse()
    {
        var path = _browserService.BrowseFolder(FolderPath);
        if (path != null)
            FolderPath = path;
    }

    [RelayCommand(CanExecute = nameof(CanLoad))]
    private void Load()
    {
        if (SelectedTabIndex == 0)
            LoadCsv();
        else
            LoadXml();
    }

    private bool CanLoad() => !string.IsNullOrWhiteSpace(FolderPath);

    private void LoadCsv()
    {
        var result = _csvLoadService.Load(FolderPath);

        if (!result.Success)
        {
            MessageBox.Show(result.ErrorMessage ?? "알 수 없는 오류", "로드 오류",
                MessageBoxButton.OK, MessageBoxImage.Error);
            IsCsvLoaded = false;
            return;
        }

        CsvTab.MainPanel.Items.Clear();
        CsvTab.MainPanel.Lines = result.MainLines;
        CsvTab.MainPanel.FileEncoding = result.MainEncoding;
        CsvTab.MainPanel.LineEnding = result.MainLineEnding;
        foreach (var entry in result.MainEntries)
            CsvTab.MainPanel.Items.Add(new MainEntryItemViewModel(entry));

        CsvTab.DriverPanel.Items.Clear();
        CsvTab.DriverPanel.Lines = result.DriverLines;
        CsvTab.DriverPanel.FileEncoding = result.DriverEncoding;
        CsvTab.DriverPanel.LineEnding = result.DriverLineEnding;
        foreach (var row in result.DriverRows)
            CsvTab.DriverPanel.Items.Add(new DriverRowItemViewModel(row));

        CsvTab.FunctionPanel.Items.Clear();
        foreach (var fm in result.FunctionFiles)
            CsvTab.FunctionPanel.Items.Add(new FunctionFileItemViewModel(fm));

        CsvTab.AlarmPanel.Items.Clear();
        foreach (var am in result.AlarmFiles)
            CsvTab.AlarmPanel.Items.Add(new AlarmFileItemViewModel(am));

        IsCsvLoaded = true;
    }

    private void LoadXml()
    {
        var result = _xmlLoadService.Load(FolderPath);

        if (!result.Success)
        {
            MessageBox.Show(result.ErrorMessage ?? "알 수 없는 오류", "로드 오류",
                MessageBoxButton.OK, MessageBoxImage.Error);
            IsXmlLoaded = false;
            return;
        }

        // Main panel
        XmlTab.MainPanel.Items.Clear();
        XmlTab.MainPanel.Document = result.MainDocument;
        XmlTab.MainPanel.FilePath = result.MainFilePath;
        XmlTab.MainPanel.FileEncoding = result.MainEncoding;
        foreach (var entry in result.MainEntries)
            XmlTab.MainPanel.Items.Add(new XmlMainEntryItemViewModel(entry));

        // Driver panel
        XmlTab.DriverPanel.Items.Clear();
        XmlTab.DriverPanel.Document = result.DriverDocument;
        XmlTab.DriverPanel.FilePath = result.DriverFilePath;
        XmlTab.DriverPanel.FileEncoding = result.DriverEncoding;
        foreach (var row in result.DriverRows)
            XmlTab.DriverPanel.Items.Add(new XmlDriverRowItemViewModel(row));

        // Function panel
        XmlTab.FunctionPanel.Items.Clear();
        foreach (var fm in result.FunctionFiles)
            XmlTab.FunctionPanel.Items.Add(new XmlFunctionFileItemViewModel(fm));

        // Alarm panel
        XmlTab.AlarmPanel.Items.Clear();
        foreach (var am in result.AlarmFiles)
            XmlTab.AlarmPanel.Items.Add(new XmlAlarmFileItemViewModel(am));

        IsXmlLoaded = true;
    }

    [RelayCommand(CanExecute = nameof(CanSave))]
    private void Save()
    {
        var dlgVm = new SaveDialogViewModel();
        string folderName = Path.GetFileName(FolderPath.TrimEnd('/', '\\'));
        dlgVm.BackupFolderName = $"{folderName}_{DateTime.Now:yyyyMMddHHmm}";

        var dlg = new Views.SaveDialogWindow(dlgVm);
        dlg.Owner = Application.Current.MainWindow;
        dlg.ShowDialog();

        if (dlgVm.DialogResult != true) return;

        string? backupName = dlgVm.NoBackup ? null : dlgVm.BackupFolderName;

        try
        {
            // Backup once (covers both CSV and XML files)
            if (!string.IsNullOrWhiteSpace(backupName))
            {
                string parentDir = Path.GetDirectoryName(FolderPath)!;
                string backupPath = Path.Combine(parentDir, backupName);
                CopyDirectory(FolderPath, backupPath);
            }

            // Save active tab only
            if (SelectedTabIndex == 0 && IsCsvLoaded)
                _csvSaveService.Save(FolderPath, null, CsvTab);
            else if (SelectedTabIndex == 1 && IsXmlLoaded)
                _xmlSaveService.Save(FolderPath, XmlTab);

            MessageBox.Show("저장이 완료되었습니다.", "저장", MessageBoxButton.OK, MessageBoxImage.Information);
        }
        catch (Exception ex)
        {
            MessageBox.Show($"저장 중 오류가 발생했습니다:\n{ex.Message}", "오류",
                MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private bool CanSave() => SelectedTabIndex == 0 ? IsCsvLoaded : IsXmlLoaded;

    private static void CopyDirectory(string sourceDir, string destDir)
    {
        Directory.CreateDirectory(destDir);
        foreach (var file in Directory.GetFiles(sourceDir))
            File.Copy(file, Path.Combine(destDir, Path.GetFileName(file)), overwrite: true);
        foreach (var dir in Directory.GetDirectories(sourceDir))
            CopyDirectory(dir, Path.Combine(destDir, Path.GetFileName(dir)));
    }
}
