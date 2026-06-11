#include "ZipArchive.h"

#include <cstdio>
#include <cstring>
#include <fstream>

namespace ZipArchive
{
namespace
{
    // ── CRC32 (standard zip polynomial 0xEDB88320) ────────────────────────────
    uint32_t Crc32(const uint8_t* data, size_t len)
    {
        static uint32_t table[256];
        static bool     ready = false;
        if (!ready)
        {
            for (uint32_t i = 0; i < 256; ++i)
            {
                uint32_t c = i;
                for (int k = 0; k < 8; ++k)
                    c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
                table[i] = c;
            }
            ready = true;
        }
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < len; ++i)
            crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
        return crc ^ 0xFFFFFFFFu;
    }

    // ── Little-endian writers (append to a byte buffer) ───────────────────────
    void PutU16(std::vector<uint8_t>& b, uint16_t v)
    {
        b.push_back((uint8_t)(v & 0xFF));
        b.push_back((uint8_t)((v >> 8) & 0xFF));
    }
    void PutU32(std::vector<uint8_t>& b, uint32_t v)
    {
        b.push_back((uint8_t)(v & 0xFF));
        b.push_back((uint8_t)((v >> 8) & 0xFF));
        b.push_back((uint8_t)((v >> 16) & 0xFF));
        b.push_back((uint8_t)((v >> 24) & 0xFF));
    }
    void PutBytes(std::vector<uint8_t>& b, const void* p, size_t n)
    {
        const uint8_t* s = static_cast<const uint8_t*>(p);
        b.insert(b.end(), s, s + n);
    }

    // ── Little-endian readers (from a raw buffer) ─────────────────────────────
    uint16_t GetU16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
    uint32_t GetU32(const uint8_t* p)
    {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
               ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }

    constexpr uint32_t kSigLocal   = 0x04034b50;  // local file header
    constexpr uint32_t kSigCentral = 0x02014b50;  // central directory file header
    constexpr uint32_t kSigEocd    = 0x06054b50;  // end of central directory
}

bool Write(const char* Path, const std::vector<Entry>& Entries)
{
    std::vector<uint8_t> out;
    struct Rec { uint32_t crc, size, offset; };
    std::vector<Rec> records;
    records.reserve(Entries.size());

    // Local file headers + data.
    for (const auto& e : Entries)
    {
        const uint32_t crc    = Crc32(e.Bytes.data(), e.Bytes.size());
        const uint32_t size   = (uint32_t)e.Bytes.size();
        const uint32_t offset = (uint32_t)out.size();
        records.push_back({ crc, size, offset });

        PutU32(out, kSigLocal);
        PutU16(out, 20);            // version needed
        PutU16(out, 0);             // flags
        PutU16(out, 0);             // method = store
        PutU16(out, 0);             // mod time
        PutU16(out, 0);             // mod date
        PutU32(out, crc);
        PutU32(out, size);          // compressed size
        PutU32(out, size);          // uncompressed size
        PutU16(out, (uint16_t)e.Name.size());
        PutU16(out, 0);             // extra length
        PutBytes(out, e.Name.data(), e.Name.size());
        PutBytes(out, e.Bytes.data(), e.Bytes.size());
    }

    // Central directory.
    const uint32_t cdStart = (uint32_t)out.size();
    for (size_t i = 0; i < Entries.size(); ++i)
    {
        const auto& e = Entries[i];
        const auto& r = records[i];
        PutU32(out, kSigCentral);
        PutU16(out, 20);            // version made by
        PutU16(out, 20);            // version needed
        PutU16(out, 0);             // flags
        PutU16(out, 0);             // method = store
        PutU16(out, 0);             // mod time
        PutU16(out, 0);             // mod date
        PutU32(out, r.crc);
        PutU32(out, r.size);        // compressed size
        PutU32(out, r.size);        // uncompressed size
        PutU16(out, (uint16_t)e.Name.size());
        PutU16(out, 0);             // extra length
        PutU16(out, 0);             // comment length
        PutU16(out, 0);             // disk number start
        PutU16(out, 0);             // internal attrs
        PutU32(out, 0);             // external attrs
        PutU32(out, r.offset);      // local header offset
        PutBytes(out, e.Name.data(), e.Name.size());
    }
    const uint32_t cdSize = (uint32_t)out.size() - cdStart;

    // End of central directory.
    PutU32(out, kSigEocd);
    PutU16(out, 0);                 // disk number
    PutU16(out, 0);                 // disk with central dir
    PutU16(out, (uint16_t)Entries.size());  // entries on this disk
    PutU16(out, (uint16_t)Entries.size());  // total entries
    PutU32(out, cdSize);
    PutU32(out, cdStart);
    PutU16(out, 0);                 // comment length

    std::ofstream f(Path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(out.data()), (std::streamsize)out.size());
    return f.good();
}

bool Read(const char* Path, std::vector<Entry>& OutEntries)
{
    OutEntries.clear();

    std::ifstream f(Path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const std::streamoff fileLen = f.tellg();
    if (fileLen < 22) return false;  // smaller than an empty EOCD
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf((size_t)fileLen);
    f.read(reinterpret_cast<char*>(buf.data()), fileLen);
    if (!f) return false;

    // Locate EOCD by scanning backward for its signature (comment is empty, so it
    // normally sits at len-22, but scan to be safe).
    size_t eocd = 0;
    bool   found = false;
    const size_t minPos = (buf.size() >= 22) ? buf.size() - 22 : 0;
    for (size_t i = minPos + 1; i-- > 0;)
    {
        if (i + 4 <= buf.size() && GetU32(&buf[i]) == kSigEocd)
        {
            eocd = i;
            found = true;
            break;
        }
        if (minPos - i > 0xFFFF) break;  // comment can't exceed 64 KB
    }
    if (!found || eocd + 22 > buf.size()) return false;

    const uint16_t total   = GetU16(&buf[eocd + 10]);
    const uint32_t cdStart = GetU32(&buf[eocd + 16]);

    size_t p = cdStart;
    for (uint16_t i = 0; i < total; ++i)
    {
        if (p + 46 > buf.size() || GetU32(&buf[p]) != kSigCentral) break;

        const uint16_t method  = GetU16(&buf[p + 10]);
        const uint32_t compSz  = GetU32(&buf[p + 20]);
        const uint16_t nameLen = GetU16(&buf[p + 28]);
        const uint16_t extraLen = GetU16(&buf[p + 30]);
        const uint16_t cmtLen  = GetU16(&buf[p + 32]);
        const uint32_t lhOff   = GetU32(&buf[p + 42]);

        std::string name;
        if (p + 46 + nameLen <= buf.size())
            name.assign(reinterpret_cast<const char*>(&buf[p + 46]), nameLen);

        // Resolve the data offset from the local header (its name/extra lengths
        // can differ from the central-directory record).
        if (method == 0 && lhOff + 30 <= buf.size() && GetU32(&buf[lhOff]) == kSigLocal)
        {
            const uint16_t lhName  = GetU16(&buf[lhOff + 26]);
            const uint16_t lhExtra = GetU16(&buf[lhOff + 28]);
            const size_t   dataAt  = (size_t)lhOff + 30 + lhName + lhExtra;
            if (dataAt + compSz <= buf.size())
            {
                Entry e;
                e.Name = name;
                e.Bytes.assign(buf.begin() + dataAt, buf.begin() + dataAt + compSz);
                OutEntries.push_back(std::move(e));
            }
        }

        p += 46 + nameLen + extraLen + cmtLen;
    }

    return true;
}

const std::vector<uint8_t>* Find(const std::vector<Entry>& Entries, const std::string& Name)
{
    for (const auto& e : Entries)
        if (e.Name == Name)
            return &e.Bytes;
    return nullptr;
}

} // namespace ZipArchive
