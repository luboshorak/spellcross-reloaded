// Minimal read-only ISO 9660 reader for original Spellcross CD images.
//
// Accepts standard 2048-byte-sector .iso images without mounting them or
// invoking external utilities. It extracts only named files from /DATA/.
// UDF-only, raw 2352-byte BIN/CUE and multi-extent files are not supported;
// the UI offers the original mounted/extracted-folder import as a fallback.
#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace spellcross_iso
{
    struct DirectoryEntry
    {
        std::string name;
        std::uint32_t sector = 0;
        std::uint32_t size = 0;
        bool directory = false;
        bool multi_extent = false;
    };

    class Reader
    {
    public:
        bool Open(const std::filesystem::path& iso, std::string& error)
        {
            error.clear();
            data_entries_.clear();
            stream_.close();
            stream_.clear();

            std::error_code ec;
            const auto length = std::filesystem::file_size(iso, ec);
            if (ec || length < 17ull * kSectorSize)
            {
                error = "The ISO file is missing or too small.";
                return false;
            }
            length_ = length;
            stream_.open(iso, std::ios::binary);
            if (!stream_)
            {
                error = "Cannot open the selected ISO file.";
                return false;
            }

            // ISO 9660 descriptors start at sector 16; a boot record may
            // precede the primary volume descriptor.
            std::array<unsigned char, kSectorSize> sector{};
            bool found_pvd = false;
            for (std::uint32_t lba = 16; lba < 48; ++lba)
            {
                if (!ReadAt(static_cast<std::uint64_t>(lba) * kSectorSize,
                            sector.data(), sector.size()))
                    break;
                if (std::string(reinterpret_cast<const char*>(sector.data() + 1), 5) != "CD001" ||
                    sector[6] != 1)
                    continue;
                if (sector[0] == 255) break;
                if (sector[0] != 1) continue;
                found_pvd = true;
                break;
            }
            if (!found_pvd)
            {
                error = "No ISO 9660 primary volume descriptor was found. "
                        "Try mounting the image in Windows and importing its DATA folder.";
                return false;
            }

            DirectoryEntry root;
            if (!ParseEntry(sector.data() + 156, sector[156], root) || !root.directory)
            {
                error = "The ISO 9660 root directory is invalid.";
                return false;
            }

            std::vector<DirectoryEntry> root_entries;
            if (!ReadDirectory(root, root_entries, error))
                return false;
            const auto data = std::find_if(root_entries.begin(), root_entries.end(),
                [](const DirectoryEntry& e) { return e.directory && e.name == "DATA"; });
            if (data == root_entries.end())
            {
                error = "The ISO has no DATA folder containing the original Spellcross installer.";
                return false;
            }
            if (!ReadDirectory(*data, data_entries_, error))
                return false;
            if (!HasDataFile("INSTALL.DTA"))
            {
                error = "DATA/INSTALL.DTA was not found in this ISO. "
                        "Select an original Spellcross installation CD image.";
                return false;
            }
            return true;
        }

        bool HasDataFile(const std::string& name) const
        {
            const std::string wanted = Upper(name);
            return std::any_of(data_entries_.begin(), data_entries_.end(),
                [&](const DirectoryEntry& e) {
                    return !e.directory && !e.multi_extent && e.name == wanted;
                });
        }

        bool ExtractDataFile(const std::string& name,
            const std::filesystem::path& destination, std::string& error)
        {
            const std::string wanted = Upper(name);
            const auto found = std::find_if(data_entries_.begin(), data_entries_.end(),
                [&](const DirectoryEntry& e) { return !e.directory && e.name == wanted; });
            if (found == data_entries_.end())
            {
                error = "File " + name + " not found in ISO DATA directory.";
                return false;
            }
            if (found->multi_extent)
            {
                error = name + " is a multi-extent ISO file; this layout is not supported.";
                return false;
            }
            const std::uint64_t offset = static_cast<std::uint64_t>(found->sector) * kSectorSize;
            if (offset > length_ || found->size > length_ - offset)
            {
                error = name + " points outside the ISO image.";
                return false;
            }

            std::error_code ec;
            std::filesystem::create_directories(destination.parent_path(), ec);
            if (ec)
            {
                error = "Cannot create the destination game-data directory: " + ec.message();
                return false;
            }
            std::ofstream out(destination, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                error = "Cannot write extracted file: " + destination.string();
                return false;
            }
            stream_.clear();
            stream_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
            std::array<char, 1024 * 1024> buffer{};
            std::uint64_t remaining = found->size;
            while (remaining)
            {
                const auto chunk = static_cast<size_t>(std::min<std::uint64_t>(
                    remaining, buffer.size()));
                stream_.read(buffer.data(), static_cast<std::streamsize>(chunk));
                if (!stream_)
                {
                    error = "Reading " + name + " from the ISO failed.";
                    return false;
                }
                out.write(buffer.data(), static_cast<std::streamsize>(chunk));
                if (!out)
                {
                    error = "Writing " + name + " to the game-data folder failed.";
                    return false;
                }
                remaining -= chunk;
            }
            out.close();
            if (!out)
            {
                error = "Cannot finish writing " + name + ".";
                return false;
            }
            return true;
        }

    private:
        static constexpr std::uint64_t kSectorSize = 2048;
        std::ifstream stream_;
        std::uint64_t length_ = 0;
        std::vector<DirectoryEntry> data_entries_;

        static std::uint32_t Le32(const unsigned char* p)
        {
            return std::uint32_t(p[0]) |
                (std::uint32_t(p[1]) << 8) |
                (std::uint32_t(p[2]) << 16) |
                (std::uint32_t(p[3]) << 24);
        }

        static std::string Upper(std::string s)
        {
            for (char& c : s)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return s;
        }

        bool ReadAt(std::uint64_t offset, void* data, size_t count)
        {
            if (offset > length_ || count > length_ - offset ||
                offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()))
                return false;
            stream_.clear();
            stream_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
            stream_.read(static_cast<char*>(data), static_cast<std::streamsize>(count));
            return static_cast<bool>(stream_);
        }

        static bool ParseEntry(const unsigned char* p, size_t available,
                               DirectoryEntry& entry)
        {
            if (available < 34 || p[0] < 34 || p[0] > available)
                return false;
            const size_t name_length = p[32];
            if (name_length == 0 || 33 + name_length > p[0])
                return false;
            entry.sector = Le32(p + 2);
            entry.size = Le32(p + 10);
            entry.directory = (p[25] & 0x02) != 0;
            entry.multi_extent = (p[25] & 0x80) != 0;
            if (name_length == 1 && (p[33] == 0 || p[33] == 1))
                entry.name.clear(); // self and parent entries
            else
            {
                entry.name.assign(reinterpret_cast<const char*>(p + 33), name_length);
                const auto version = entry.name.find(';');
                if (version != std::string::npos)
                    entry.name.resize(version);
                if (!entry.name.empty() && entry.name.back() == '.')
                    entry.name.pop_back();
                entry.name = Upper(entry.name);
            }
            return true;
        }

        bool ReadDirectory(const DirectoryEntry& dir,
            std::vector<DirectoryEntry>& entries, std::string& error)
        {
            entries.clear();
            constexpr std::uint64_t max_directory = 16ull * 1024ull * 1024ull;
            const std::uint64_t offset = static_cast<std::uint64_t>(dir.sector) * kSectorSize;
            if (!dir.directory || dir.multi_extent || dir.size == 0 ||
                dir.size > max_directory || offset > length_ || dir.size > length_ - offset)
            {
                error = "Invalid or unsupported ISO directory layout.";
                return false;
            }
            std::vector<unsigned char> bytes(dir.size);
            if (!ReadAt(offset, bytes.data(), bytes.size()))
            {
                error = "Cannot read a directory from the ISO.";
                return false;
            }
            for (size_t pos = 0; pos < bytes.size();)
            {
                const auto length = bytes[pos];
                if (length == 0)
                {
                    pos = ((pos / kSectorSize) + 1) * kSectorSize;
                    continue;
                }
                if (length < 34 || pos + length > bytes.size() ||
                    (pos % kSectorSize) + length > kSectorSize)
                {
                    error = "Invalid ISO directory record.";
                    return false;
                }
                DirectoryEntry entry;
                if (!ParseEntry(bytes.data() + pos, length, entry))
                {
                    error = "Corrupt ISO directory entry.";
                    return false;
                }
                if (!entry.name.empty()) entries.push_back(std::move(entry));
                pos += length;
            }
            return true;
        }
    };
} // namespace spellcross_iso
