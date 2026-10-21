#include "WinIo.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>

namespace alarm
{
    namespace
    {
        class Handle
        {
        public:
            explicit Handle(HANDLE value) : value_(value) {}
            ~Handle() { if (value_ != INVALID_HANDLE_VALUE) CloseHandle(value_); }
            Handle(const Handle&) = delete;
            Handle& operator=(const Handle&) = delete;
            HANDLE get() const { return value_; }
        private:
            HANDLE value_;
        };

        class SearchHandle
        {
        public:
            explicit SearchHandle(HANDLE value) : value_(value) {}
            ~SearchHandle() { if (value_ != INVALID_HANDLE_VALUE) FindClose(value_); }
            HANDLE get() const { return value_; }
        private:
            HANDLE value_;
        };

        std::wstring SystemError(const std::wstring& operation, const std::wstring& path, DWORD code = GetLastError())
        {
            wchar_t buffer[1024] = {};
            FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code,
                0, buffer, 1024, nullptr);
            return operation + L": " + path + L" (Win32 " + std::to_wstring(code) + L": " + Trim(buffer) + L")";
        }

        std::wstring Native(const std::wstring& path)
        {
            if (path.compare(0, 4, L"\\\\?\\") == 0) return path;
            if (path.compare(0, 2, L"\\\\") == 0) return L"\\\\?\\UNC\\" + path.substr(2);
            return L"\\\\?\\" + path;
        }

        std::wstring Display(const std::wstring& path)
        {
            if (path.compare(0, 8, L"\\\\?\\UNC\\") == 0) return L"\\\\" + path.substr(8);
            if (path.compare(0, 4, L"\\\\?\\") == 0) return path.substr(4);
            return path;
        }

        bool EqualPath(const std::wstring& a, const std::wstring& b)
        {
            return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
        }

        void RejectReparse(HANDLE handle, const std::wstring& path, bool directory)
        {
            FILE_ATTRIBUTE_TAG_INFO info = {};
            if (!GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &info, sizeof(info)))
                throw IoError(SystemError(L"Inspect", path));
            if (info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                throw IoError(L"Links/junctions are not supported: " + path);
            if (!!(info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != directory)
                throw IoError(L"Unexpected file type: " + path);
        }

        void WriteAll(HANDLE handle, const Bytes& bytes, const std::wstring& path)
        {
            size_t offset = 0;
            while (offset < bytes.size())
            {
                DWORD size = static_cast<DWORD>((std::min)(bytes.size() - offset, size_t(1024 * 1024)));
                DWORD written = 0;
                if (!WriteFile(handle, bytes.data() + offset, size, &written, nullptr) || written != size)
                    throw IoError(SystemError(L"Write", path));
                offset += written;
            }
            if (!FlushFileBuffers(handle)) throw IoError(SystemError(L"Flush", path));
        }

        void Enumerate(Snapshot& snapshot, const std::wstring& relative)
        {
            const auto path = relative.empty() ? snapshot.root : JoinPath(snapshot.root, relative);
            // Keep the directory open without delete sharing while it is enumerated.
            Handle guard(CreateFileW(Native(path).c_str(), FILE_LIST_DIRECTORY,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
            if (guard.get() == INVALID_HANDLE_VALUE) throw IoError(SystemError(L"Open directory", path));
            RejectReparse(guard.get(), path, true);
            WIN32_FIND_DATAW entry = {};
            SearchHandle search(FindFirstFileW(Native(JoinPath(path, L"*")).c_str(), &entry));
            if (search.get() == INVALID_HANDLE_VALUE)
            {
                DWORD error = GetLastError();
                if (error == ERROR_FILE_NOT_FOUND) return;
                throw IoError(SystemError(L"Enumerate directory", path, error));
            }
            do
            {
                std::wstring name = entry.cFileName;
                if (name == L"." || name == L"..") continue;
                auto rel = relative.empty() ? name : JoinPath(relative, name);
                auto full = JoinPath(snapshot.root, rel);
                if (entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                {
                    snapshot.errors.push_back(L"Links/junctions are not supported: " + full);
                    continue;
                }
                try
                {
                    if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    {
                        snapshot.directories.push_back(rel);
                        Enumerate(snapshot, rel);
                    }
                    else
                    {
                        SnapshotFile file{ rel, ReadBytes(full) };
                        if (!snapshot.files.emplace(rel, std::move(file)).second)
                            snapshot.errors.push_back(L"Ambiguous case-insensitive relative path: " + rel);
                    }
                }
                catch (const std::exception& error) { snapshot.errors.push_back(ExceptionMessage(error)); }
            } while (FindNextFileW(search.get(), &entry));
            DWORD error = GetLastError();
            if (error != ERROR_NO_MORE_FILES) throw IoError(SystemError(L"Continue enumeration", path, error));
        }

        void RemoveTree(const std::wstring& path)
        {
            DWORD attributes = GetFileAttributesW(Native(path).c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES) throw IoError(SystemError(L"Inspect cleanup path", path));
            if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) || !(attributes & FILE_ATTRIBUTE_DIRECTORY))
                throw IoError(L"Refusing unsafe cleanup path: " + path);
            WIN32_FIND_DATAW entry = {};
            {
                SearchHandle search(FindFirstFileW(Native(JoinPath(path, L"*")).c_str(), &entry));
                if (search.get() == INVALID_HANDLE_VALUE)
                {
                    if (GetLastError() != ERROR_FILE_NOT_FOUND) throw IoError(SystemError(L"Enumerate cleanup", path));
                }
                else
                {
                    do
                    {
                        std::wstring name = entry.cFileName;
                        if (name == L"." || name == L"..") continue;
                        auto child = JoinPath(path, name);
                        if (entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                            throw IoError(L"Refusing to clean reparse point: " + child);
                        if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) RemoveTree(child);
                        else if (!DeleteFileW(Native(child).c_str())) throw IoError(SystemError(L"Cleanup file", child));
                    } while (FindNextFileW(search.get(), &entry));
                    if (GetLastError() != ERROR_NO_MORE_FILES) throw IoError(SystemError(L"Continue cleanup", path));
                }
            }
            if (!RemoveDirectoryW(Native(path).c_str())) throw IoError(SystemError(L"Cleanup directory", path));
        }
    }

    bool PathLess::operator()(const std::wstring& a, const std::wstring& b) const
    {
        return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_LESS_THAN;
    }

    std::wstring JoinPath(const std::wstring& parent, const std::wstring& child)
    {
        return parent + (parent.empty() || parent.back() == L'\\' ? L"" : L"\\") + child;
    }

    std::wstring AbsolutePath(const std::wstring& input)
    {
        auto path = Trim(input);
        if (path.size() >= 2 && path.front() == L'"' && path.back() == L'"') path = path.substr(1, path.size() - 2);
        std::replace(path.begin(), path.end(), L'/', L'\\');
        path = Display(path);
        bool drive = path.size() >= 3 && ((path[0] >= L'A' && path[0] <= L'Z') ||
            (path[0] >= L'a' && path[0] <= L'z')) && path[1] == L':' && path[2] == L'\\';
        bool unc = path.size() > 2 && path.compare(0, 2, L"\\\\") == 0;
        if ((!drive && !unc) || path.find(L'\0') != std::wstring::npos ||
            path.compare(0, 4, L"\\\\.\\") == 0 || path.compare(0, 4, L"\\\\?\\") == 0)
            throw IoError(L"Enter an absolute drive or UNC folder path.");
        DWORD length = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
        if (!length) throw IoError(SystemError(L"Resolve path", path));
        std::wstring full(length, L'\0');
        DWORD written = GetFullPathNameW(path.c_str(), length, &full[0], nullptr);
        if (!written || written >= length) throw IoError(SystemError(L"Resolve path", path));
        full.resize(written);
        while (full.size() > 3 && full.back() == L'\\') full.pop_back();
        return full;
    }

    std::wstring CanonicalDirectory(const std::wstring& path)
    {
        const auto absolute = AbsolutePath(path);
        Handle handle(CreateFileW(Native(absolute).c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        if (handle.get() == INVALID_HANDLE_VALUE) throw IoError(SystemError(L"Open directory", absolute));
        RejectReparse(handle.get(), absolute, true);
        DWORD length = GetFinalPathNameByHandleW(handle.get(), nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        if (!length) throw IoError(SystemError(L"Canonicalize directory", absolute));
        std::wstring result(length + 1, L'\0');
        DWORD written = GetFinalPathNameByHandleW(handle.get(), &result[0], length + 1, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        if (!written || written > length) throw IoError(SystemError(L"Canonicalize directory", absolute));
        result.resize(written);
        result = Display(result);
        while (result.size() > 3 && result.back() == L'\\') result.pop_back();
        return result;
    }

    std::wstring ExecutableDirectory()
    {
        std::vector<wchar_t> buffer(1024);
        for (;;)
        {
            DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (!size) throw IoError(SystemError(L"Locate executable", L""));
            if (size < buffer.size())
            {
                std::wstring path(buffer.data(), size);
                return CanonicalDirectory(path.substr(0, path.find_last_of(L"\\/")));
            }
            buffer.resize(buffer.size() * 2);
        }
    }

    bool IsWithin(const std::wstring& path, const std::wstring& directory)
    {
        if (EqualPath(path, directory)) return true;
        auto prefix = directory + (directory.back() == L'\\' ? L"" : L"\\");
        return path.size() >= prefix.size() && EqualPath(path.substr(0, prefix.size()), prefix);
    }

    bool Exists(const std::wstring& path)
    {
        if (GetFileAttributesW(Native(path).c_str()) != INVALID_FILE_ATTRIBUTES) return true;
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return false;
        throw IoError(SystemError(L"Inspect path", path, error));
    }

    bool IsCsv(const std::wstring& path)
    {
        return path.size() >= 4 && EqualPath(path.substr(path.size() - 4), L".csv");
    }

    std::wstring Timestamp()
    {
        SYSTEMTIME time = {};
        GetLocalTime(&time);
        std::wostringstream out;
        out << std::setfill(L'0') << std::setw(4) << time.wYear << std::setw(2) << time.wMonth
            << std::setw(2) << time.wDay << L"_" << std::setw(2) << time.wHour
            << std::setw(2) << time.wMinute << std::setw(2) << time.wSecond;
        return out.str();
    }

    std::wstring ExceptionMessage(const std::exception& error)
    {
        auto io = dynamic_cast<const IoError*>(&error);
        if (io) return io->detail;
        std::string message = error.what();
        return std::wstring(message.begin(), message.end());
    }

    Bytes ReadBytes(const std::wstring& path)
    {
        // Deny writers/deletion while snapshotting, and never follow a file link.
        Handle handle(CreateFileW(Native(path).c_str(), GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
        if (handle.get() == INVALID_HANDLE_VALUE) throw IoError(SystemError(L"Read", path));
        RejectReparse(handle.get(), path, false);
        LARGE_INTEGER size = {};
        if (!GetFileSizeEx(handle.get(), &size)) throw IoError(SystemError(L"Read size", path));
        if (size.QuadPart < 0 || static_cast<unsigned long long>(size.QuadPart) > (std::numeric_limits<size_t>::max)())
            throw IoError(L"File too large: " + path);
        Bytes bytes(static_cast<size_t>(size.QuadPart));
        size_t offset = 0;
        while (offset < bytes.size())
        {
            DWORD chunk = static_cast<DWORD>((std::min)(bytes.size() - offset, size_t(1024 * 1024)));
            DWORD read = 0;
            if (!ReadFile(handle.get(), bytes.data() + offset, chunk, &read, nullptr) || read != chunk)
                throw IoError(SystemError(L"Read bytes", path));
            offset += read;
        }
        return bytes;
    }

    Snapshot TakeSnapshot(const std::wstring& directory)
    {
        Snapshot result;
        result.root = directory;
        try { Enumerate(result, L""); }
        catch (const std::exception& error) { result.errors.push_back(ExceptionMessage(error)); }
        return result;
    }

    void WriteNewBytes(const std::wstring& path, const Bytes& bytes)
    {
        // CREATE_NEW is essential: neither sources nor previous results may be overwritten.
        bool created = false;
        try
        {
            Handle handle(CreateFileW(Native(path).c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
            if (handle.get() == INVALID_HANDLE_VALUE) throw IoError(SystemError(L"Create file", path));
            created = true;
            WriteAll(handle.get(), bytes, path);
        }
        catch (...)
        {
            if (created) DeleteFileW(Native(path).c_str());
            throw;
        }
    }

    void AppendReport(const std::wstring& path, const std::wstring& text)
    {
        Handle handle(CreateFileW(Native(path).c_str(), FILE_APPEND_DATA, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        if (handle.get() == INVALID_HANDLE_VALUE) throw IoError(SystemError(L"Append report", path));
        RejectReparse(handle.get(), path, false);
        WriteAll(handle.get(), EncodeText(text, Encoding::Utf8, false), path);
    }

    std::wstring WriteUniqueReport(const std::wstring& directory, const std::wstring& text)
    {
        const auto base = L"AlarmDiff_" + Timestamp();
        const auto bytes = EncodeText(text, Encoding::Utf8, true);
        for (unsigned int n = 0; n < 100000; ++n)
        {
            auto path = JoinPath(directory, base + (n ? L"_" + std::to_wstring(n) : L"") + L".txt");
            Handle handle(CreateFileW(Native(path).c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
            if (handle.get() == INVALID_HANDLE_VALUE)
            {
                DWORD error = GetLastError();
                if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS) continue;
                throw IoError(SystemError(L"Create report", path, error));
            }
            WriteAll(handle.get(), bytes, path);
            return path;
        }
        throw IoError(L"Unable to allocate unique report name");
    }

    void CreateDirectoryNew(const std::wstring& path)
    {
        if (!CreateDirectoryW(Native(path).c_str(), nullptr)) throw IoError(SystemError(L"Create directory", path));
    }

    std::wstring CreateStagingDirectory(const std::wstring& directory)
    {
        auto base = L"Alarm.pending_" + Timestamp() + L"_" + std::to_wstring(GetCurrentProcessId());
        for (unsigned int n = 0; n < 100000; ++n)
        {
            auto path = JoinPath(directory, base + L"_" + std::to_wstring(n));
            if (CreateDirectoryW(Native(path).c_str(), nullptr)) return path;
            DWORD error = GetLastError();
            if (error != ERROR_ALREADY_EXISTS) throw IoError(SystemError(L"Create staging directory", path, error));
        }
        throw IoError(L"Unable to allocate staging directory");
    }

    std::wstring PublishDirectory(const std::wstring& staging, const std::wstring& directory,
        const std::wstring& baseName)
    {
        if (baseName.empty() || baseName == L"." || baseName == L".." ||
            baseName.find_first_of(L"\\/:*?\"<>|") != std::wstring::npos)
            throw IoError(L"Invalid output folder name: " + baseName);
        auto base = baseName + L"_" + Timestamp();
        for (unsigned int n = 0; n < 100000; ++n)
        {
            auto name = n == 0 ? baseName : base + (n == 1 ? L"" : L"_" + std::to_wstring(n - 1));
            auto path = JoinPath(directory, name);
            if (Exists(path)) continue;
            if (MoveFileExW(Native(staging).c_str(), Native(path).c_str(), MOVEFILE_WRITE_THROUGH)) return path;
            DWORD error = GetLastError();
            if (error != ERROR_ALREADY_EXISTS && error != ERROR_FILE_EXISTS)
                throw IoError(SystemError(L"Publish result", path, error));
        }
        throw IoError(L"Unable to allocate output folder name");
    }

    void RemoveOwnedTree(const std::wstring& path, const std::wstring& allowedParent)
    {
        auto absolute = AbsolutePath(path);
        auto parent = absolute.substr(0, absolute.find_last_of(L'\\'));
        if (!EqualPath(CanonicalDirectory(parent), CanonicalDirectory(allowedParent)) ||
            !IsWithin(CanonicalDirectory(absolute), CanonicalDirectory(allowedParent)) ||
            EqualPath(CanonicalDirectory(absolute), CanonicalDirectory(allowedParent)))
            throw IoError(L"Refusing cleanup outside the owned output directory: " + path);
        RemoveTree(absolute);
    }
}
