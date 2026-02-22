using Microsoft.Win32;
using System.IO;

namespace SCTC_CONFIG.Services;

public class FolderBrowserService
{
    public string? BrowseFolder(string? initialPath = null)
    {
        var dialog = new OpenFolderDialog
        {
            Title = "폴더 선택",
            Multiselect = false
        };

        if (!string.IsNullOrEmpty(initialPath) && Directory.Exists(initialPath))
            dialog.InitialDirectory = initialPath;

        return dialog.ShowDialog() == true ? dialog.FolderName : null;
    }
}
