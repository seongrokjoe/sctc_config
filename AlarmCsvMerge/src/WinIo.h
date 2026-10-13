#pragma once

#include "AlarmCore.h"
#include <stdexcept>

namespace alarm
{
    class IoError : public std::runtime_error
    {
    public:
        explicit IoError(const std::wstring& message) : std::runtime_error("Windows I/O error"), detail(message) {}
        std::wstring detail;
    };

    struct PathLess
    {
        bool operator()(const std::wstring& a, const std::wstring& b) const;
    };

    struct SnapshotFile
    {
        std::wstring relative;
        Bytes bytes;
    };

    struct Snapshot
    {
        std::wstring root;
        std::vector<std::wstring> directories;
        std::map<std::wstring, SnapshotFile, PathLess> files;
        std::vector<std::wstring> errors;
    };

    std::wstring JoinPath(const std::wstring& parent, const std::wstring& child);
    std::wstring AbsolutePath(const std::wstring& input);
    std::wstring CanonicalDirectory(const std::wstring& path);
    std::wstring ExecutableDirectory();
    bool IsWithin(const std::wstring& path, const std::wstring& directory);
    bool Exists(const std::wstring& path);
    bool IsCsv(const std::wstring& path);
    std::wstring Timestamp();
    std::wstring ExceptionMessage(const std::exception& error);
    Snapshot TakeSnapshot(const std::wstring& directory);
    Bytes ReadBytes(const std::wstring& path);
    void WriteNewBytes(const std::wstring& path, const Bytes& bytes);
    void AppendReport(const std::wstring& path, const std::wstring& text);
    std::wstring WriteUniqueReport(const std::wstring& directory, const std::wstring& text);
    void CreateDirectoryNew(const std::wstring& path);
    std::wstring CreateStagingDirectory(const std::wstring& directory);
    std::wstring PublishDirectory(const std::wstring& staging, const std::wstring& directory);
    // Only remove a tree created by this run. Reparse points are refused.
    void RemoveOwnedTree(const std::wstring& path, const std::wstring& allowedParent);
}
