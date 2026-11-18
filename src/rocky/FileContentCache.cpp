/** rocky c++; Copyright 2026 Pelican Mapping; MIT License */
#include "FileContentCache.h"
#include "Context.h"
#include <fstream>
#include <sstream>
#include <random>
#include <stdexcept>

using namespace ROCKY_NAMESPACE;
namespace
{
    constexpr std::uint64_t magic = 0x3145484341434b52ull; // RKCACHE1
    constexpr std::uint64_t maxData = 256u * 1024u * 1024u, maxText = 1024u * 1024u;

    //! Computes a stable checksum/filename hash; stored keys independently protect against filename collisions.
    std::uint64_t hash(std::string_view bytes, std::uint64_t h = 14695981039346656037ull)
    {
        for (unsigned char c : bytes) { h ^= c; h *= 1099511628211ull; }
        return h;
    }

    //! Encodes an integer in portable little-endian form.
    void write64(std::ostream& out, std::uint64_t value)
    {
        for (unsigned i = 0; i < 8; ++i) { out.put(static_cast<char>(value & 255u)); value >>= 8; }
    }

    //! Decodes an integer, leaving short reads in the stream's failure state.
    std::uint64_t read64(std::istream& in)
    {
        std::uint64_t value = 0;
        for (unsigned i = 0; i < 8; ++i)
            value |= static_cast<std::uint64_t>(static_cast<unsigned char>(in.get())) << (i * 8);
        return value;
    }

    //! Creates a stable filename without exposing URI path/query characters.
    std::string filename(const std::string& key)
    {
        std::ostringstream out;
        out << std::hex << hash(key) << ".rkc";
        return out.str();
    }

    //! Uniquely names temporary files across cache instances and processes.
    std::string temporarySuffix()
    {
        static const auto nonce = []() {
            std::random_device random;
            return (static_cast<std::uint64_t>(random()) << 32) ^ random();
        }();
        static std::atomic_uint64_t serial{0};
        return "." + std::to_string(nonce) + "." + std::to_string(++serial) + ".tmp";
    }

    //! Checks payload, key, type and timestamp together.
    std::uint64_t checksum(const std::string& key, const Content& c, std::uint64_t stamp)
    {
        return hash(c.data, hash(c.type, hash(key))) ^ stamp;
    }
}

FileContentCache::FileContentCache(const std::string& directory, std::size_t memoryEntries) :
    _directory(directory),
    _path(std::filesystem::absolute(std::filesystem::path(directory)) / "rocky-content-v1"),
    _memory(memoryEntries)
{
    if (directory.empty()) throw std::invalid_argument("FileContentCache requires a nonempty directory");
}

void FileContentCache::warn(const std::string& message)
{
    if (!_warned) { Log()->warn("Content cache: {}", message); _warned = true; }
}

std::optional<Result<Content>> FileContentCache::get(const std::string& key)
{
    std::scoped_lock lock(_mutex);
    if (auto cached = _memory.get(key)) { ++_hits; return cached; }
    std::ifstream in(_path / filename(key), std::ios::binary);
    if (in)
    {
        const auto version = read64(in), keySize = read64(in), typeSize = read64(in), dataSize = read64(in);
        const auto stamp = read64(in), expected = read64(in);
        std::error_code ec;
        const auto fileSize = std::filesystem::file_size(_path / filename(key), ec);
        if (in && !ec && version == magic && keySize == key.size() && keySize <= maxText &&
            typeSize <= maxText && dataSize <= maxData && fileSize == 48u + keySize + typeSize + dataSize)
        {
            std::string storedKey(static_cast<std::size_t>(keySize), '\0');
            Content c;
            c.type.resize(static_cast<std::size_t>(typeSize));
            c.data.resize(static_cast<std::size_t>(dataSize));
            in.read(storedKey.data(), static_cast<std::streamsize>(keySize));
            in.read(c.type.data(), static_cast<std::streamsize>(typeSize));
            in.read(c.data.data(), static_cast<std::streamsize>(dataSize));
            if (in && storedKey == key && checksum(key, c, stamp) == expected)
            {
                c.timestamp = std::chrono::system_clock::time_point(
                    std::chrono::duration_cast<std::chrono::system_clock::duration>(
                        std::chrono::milliseconds(static_cast<std::int64_t>(stamp))));
                Result<Content> result(std::move(c));
                _memory.put(key, result);
                ++_hits;
                return result;
            }
        }
        warn("Ignoring an invalid disk record");
    }
    ++_misses;
    return {};
}

void FileContentCache::put(const std::string& key, const Result<Content>& value)
{
    if (value.failed()) return;
    std::scoped_lock lock(_mutex);
    _memory.put(key, value);
    const auto& c = value.value();
    if (key.size() > maxText || c.type.size() > maxText || c.data.size() > maxData) return;
    std::error_code ec;
    std::filesystem::create_directories(_path, ec);
    if (ec) { warn(ec.message()); return; }
    const auto target = _path / filename(key);
    const auto temporary = std::filesystem::path(target.string() + temporarySuffix());
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    const auto stamp = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(c.timestamp.time_since_epoch()).count());
    write64(out, magic); write64(out, key.size()); write64(out, c.type.size()); write64(out, c.data.size());
    write64(out, stamp); write64(out, checksum(key, c, stamp));
    out.write(key.data(), static_cast<std::streamsize>(key.size()));
    out.write(c.type.data(), static_cast<std::streamsize>(c.type.size()));
    out.write(c.data.data(), static_cast<std::streamsize>(c.data.size()));
    out.close();
    if (out)
    {
        std::filesystem::rename(temporary, target, ec);
        if (ec)
        {
            // Windows cannot rename over an existing record. Readers see a complete record or a miss.
            std::filesystem::remove(target, ec);
            ec.clear();
            std::filesystem::rename(temporary, target, ec);
        }
        if (ec) warn(ec.message());
    }
    else warn("Cannot write disk record");
    std::filesystem::remove(temporary, ec);
}

void FileContentCache::clear()
{
    std::scoped_lock lock(_mutex);
    _memory.clear();
    _hits = 0; _misses = 0;
    std::error_code ec;
    if (!std::filesystem::exists(_path, ec)) return;
    for (std::filesystem::directory_iterator i(_path, ec), end; !ec && i != end; i.increment(ec))
    {
        if (i->path().extension() == ".rkc" && i->is_regular_file(ec))
        {
            std::filesystem::remove(i->path(), ec);
            if (ec) break;
        }
    }
    if (ec) warn(ec.message());
}
