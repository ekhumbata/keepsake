// Raw DEFLATE decoder (RFC 1951): stored, fixed-Huffman and dynamic-Huffman blocks.
// Canonical Huffman codes are decoded bit by bit from per-length counts, which is compact and
// easy to bound. Every read is range-checked: a hostile or truncated stream ends in
// kCorrupt, never in an out-of-bounds access.
#include "epub_engine/inflate.h"

#include <array>
#include <memory>

namespace epub {
namespace {

constexpr int kMaxBits = 15;
constexpr int kMaxLiteralCodes = 288;
constexpr int kMaxDistanceCodes = 30;

class BitReader {
public:
    BitReader(const uint8_t* data, size_t length) : data_(data), length_(length) {}

    // Reads `count` (0..16) bits, least significant first. Sets overrun() past the end.
    uint32_t Bits(int count)
    {
        uint32_t value = buffer_;
        while (bit_count_ < count) {
            if (position_ >= length_) {
                overrun_ = true;
                return 0;
            }
            value |= static_cast<uint32_t>(data_[position_++]) << bit_count_;
            bit_count_ += 8;
        }
        buffer_ = count >= 32 ? 0 : value >> count;
        bit_count_ -= count;
        return count == 0 ? 0 : value & ((1u << count) - 1u);
    }

    // Drops the bits left in the current byte (stored blocks start byte-aligned).
    void AlignToByte()
    {
        buffer_ = 0;
        bit_count_ = 0;
    }

    bool ReadByte(uint8_t* value)
    {
        if (position_ >= length_) {
            overrun_ = true;
            return false;
        }
        *value = data_[position_++];
        return true;
    }

    size_t remaining_bytes() const { return length_ - position_; }
    const uint8_t* current() const { return data_ + position_; }
    void Skip(size_t bytes) { position_ += bytes; }
    bool overrun() const { return overrun_; }

private:
    const uint8_t* data_;
    size_t length_;
    size_t position_ = 0;
    uint32_t buffer_ = 0;
    int bit_count_ = 0;
    bool overrun_ = false;
};

struct Huffman {
    std::array<uint16_t, kMaxBits + 1> count = {};
    std::array<uint16_t, kMaxLiteralCodes> symbol = {};
};

// Builds a canonical code from code lengths. Returns false for an over-subscribed (invalid)
// set; an incomplete set is allowed, and a code that falls in the gap fails at decode time.
bool BuildHuffman(Huffman* h, const uint8_t* lengths, int n)
{
    h->count.fill(0);
    for (int i = 0; i < n; ++i) {
        h->count[lengths[i]]++;
    }
    if (h->count[0] == n) {
        return true;  // no codes at all; any decode fails
    }
    int left = 1;
    for (int len = 1; len <= kMaxBits; ++len) {
        left <<= 1;
        left -= h->count[len];
        if (left < 0) {
            return false;
        }
    }
    std::array<uint16_t, kMaxBits + 1> offsets = {};
    for (int len = 1; len < kMaxBits; ++len) {
        offsets[len + 1] = static_cast<uint16_t>(offsets[len] + h->count[len]);
    }
    for (int i = 0; i < n; ++i) {
        if (lengths[i] != 0) {
            h->symbol[offsets[lengths[i]]++] = static_cast<uint16_t>(i);
        }
    }
    return true;
}

// Returns the decoded symbol, or -1 for an unused code or a truncated stream.
int Decode(BitReader& reader, const Huffman& h)
{
    int code = 0;
    int first = 0;
    int index = 0;
    for (int len = 1; len <= kMaxBits; ++len) {
        code |= static_cast<int>(reader.Bits(1));
        if (reader.overrun()) {
            return -1;
        }
        const int count = h.count[len];
        if (code - first < count) {
            return h.symbol[index + (code - first)];
        }
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

constexpr std::array<uint16_t, 29> kLengthBase = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,
                                                  15, 17, 19, 23, 27, 31, 35, 43, 51,  59,
                                                  67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::array<uint8_t, 29> kLengthExtra = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                                  2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr std::array<uint16_t, 30> kDistanceBase = {
    1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr std::array<uint8_t, 30> kDistanceExtra = {0, 0, 0, 0, 1, 1, 2,  2,  3,  3,
                                                    4, 4, 5, 5, 6, 6, 7,  7,  8,  8,
                                                    9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

InflateStatus InflateCodes(BitReader& reader, const Huffman& literals, const Huffman& distances,
                           size_t max_output, std::string* out)
{
    while (true) {
        const int symbol = Decode(reader, literals);
        if (symbol < 0) {
            return InflateStatus::kCorrupt;
        }
        if (symbol < 256) {
            if (out->size() >= max_output) {
                return InflateStatus::kTooLarge;
            }
            out->push_back(static_cast<char>(symbol));
            continue;
        }
        if (symbol == 256) {
            return InflateStatus::kOk;
        }
        const int length_index = symbol - 257;
        if (length_index >= static_cast<int>(kLengthBase.size())) {
            return InflateStatus::kCorrupt;
        }
        const size_t length = kLengthBase[length_index] + reader.Bits(kLengthExtra[length_index]);
        const int distance_symbol = Decode(reader, distances);
        if (distance_symbol < 0 || distance_symbol >= static_cast<int>(kDistanceBase.size())) {
            return InflateStatus::kCorrupt;
        }
        const size_t distance =
            kDistanceBase[distance_symbol] + reader.Bits(kDistanceExtra[distance_symbol]);
        if (reader.overrun() || distance > out->size()) {
            return InflateStatus::kCorrupt;
        }
        if (length > max_output - out->size()) {
            return InflateStatus::kTooLarge;
        }
        // Byte by byte: a match may overlap the bytes it is producing (distance < length).
        size_t from = out->size() - distance;
        for (size_t i = 0; i < length; ++i) {
            out->push_back((*out)[from++]);
        }
    }
}

InflateStatus InflateStored(BitReader& reader, size_t max_output, std::string* out)
{
    reader.AlignToByte();
    uint8_t header[4] = {};
    for (uint8_t& byte : header) {
        if (!reader.ReadByte(&byte)) {
            return InflateStatus::kCorrupt;
        }
    }
    const unsigned length = header[0] | (header[1] << 8);
    const unsigned complement = header[2] | (header[3] << 8);
    if (length != (~complement & 0xFFFFu)) {
        return InflateStatus::kCorrupt;
    }
    if (length > reader.remaining_bytes()) {
        return InflateStatus::kCorrupt;
    }
    if (length > max_output - out->size()) {
        return InflateStatus::kTooLarge;
    }
    out->append(reinterpret_cast<const char*>(reader.current()), length);
    reader.Skip(length);
    return InflateStatus::kOk;
}

InflateStatus InflateFixed(BitReader& reader, size_t max_output, std::string* out)
{
    std::array<uint8_t, kMaxLiteralCodes> lengths = {};
    for (int i = 0; i < 144; ++i) lengths[i] = 8;
    for (int i = 144; i < 256; ++i) lengths[i] = 9;
    for (int i = 256; i < 280; ++i) lengths[i] = 7;
    for (int i = 280; i < kMaxLiteralCodes; ++i) lengths[i] = 8;
    // Tables on the heap: this runs on small task stacks (the input task has 8 KB).
    auto tables = std::make_unique<std::array<Huffman, 2>>();
    Huffman& literals = (*tables)[0];
    BuildHuffman(&literals, lengths.data(), kMaxLiteralCodes);

    std::array<uint8_t, kMaxDistanceCodes> distance_lengths = {};
    distance_lengths.fill(5);
    Huffman& distances = (*tables)[1];
    BuildHuffman(&distances, distance_lengths.data(), kMaxDistanceCodes);
    return InflateCodes(reader, literals, distances, max_output, out);
}

InflateStatus InflateDynamic(BitReader& reader, size_t max_output, std::string* out)
{
    const int literal_count = static_cast<int>(reader.Bits(5)) + 257;
    const int distance_count = static_cast<int>(reader.Bits(5)) + 1;
    const int code_length_count = static_cast<int>(reader.Bits(4)) + 4;
    if (reader.overrun() || literal_count > 286 || distance_count > 30) {
        return InflateStatus::kCorrupt;
    }

    constexpr std::array<uint8_t, 19> kOrder = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                                11, 4,  12, 3, 13, 2, 14, 1, 15};
    std::array<uint8_t, 19> code_lengths = {};
    for (int i = 0; i < code_length_count; ++i) {
        code_lengths[kOrder[i]] = static_cast<uint8_t>(reader.Bits(3));
    }
    if (reader.overrun()) {
        return InflateStatus::kCorrupt;
    }
    auto tables = std::make_unique<std::array<Huffman, 3>>();  // heap: small task stacks
    Huffman& code_length_code = (*tables)[2];
    if (!BuildHuffman(&code_length_code, code_lengths.data(), 19)) {
        return InflateStatus::kCorrupt;
    }

    std::array<uint8_t, 286 + 30> lengths = {};
    int index = 0;
    const int total = literal_count + distance_count;
    while (index < total) {
        const int symbol = Decode(reader, code_length_code);
        if (symbol < 0) {
            return InflateStatus::kCorrupt;
        }
        if (symbol < 16) {
            lengths[index++] = static_cast<uint8_t>(symbol);
            continue;
        }
        uint8_t value = 0;
        int repeat = 0;
        if (symbol == 16) {
            if (index == 0) {
                return InflateStatus::kCorrupt;  // nothing to repeat
            }
            value = lengths[index - 1];
            repeat = 3 + static_cast<int>(reader.Bits(2));
        } else if (symbol == 17) {
            repeat = 3 + static_cast<int>(reader.Bits(3));
        } else {
            repeat = 11 + static_cast<int>(reader.Bits(7));
        }
        if (reader.overrun() || index + repeat > total) {
            return InflateStatus::kCorrupt;
        }
        while (repeat-- > 0) {
            lengths[index++] = value;
        }
    }
    if (lengths[256] == 0) {
        return InflateStatus::kCorrupt;  // no end-of-block code
    }

    Huffman& literals = (*tables)[0];
    Huffman& distances = (*tables)[1];
    if (!BuildHuffman(&literals, lengths.data(), literal_count) ||
        !BuildHuffman(&distances, lengths.data() + literal_count, distance_count)) {
        return InflateStatus::kCorrupt;
    }
    return InflateCodes(reader, literals, distances, max_output, out);
}

}  // namespace

InflateStatus InflateRaw(const uint8_t* input, size_t input_length, size_t max_output,
                         std::string* out)
{
    if (out == nullptr || (input == nullptr && input_length > 0)) {
        return InflateStatus::kCorrupt;
    }
    BitReader reader(input, input_length);
    bool last = false;
    while (!last) {
        last = reader.Bits(1) != 0;
        const uint32_t type = reader.Bits(2);
        if (reader.overrun()) {
            return InflateStatus::kCorrupt;
        }
        InflateStatus status = InflateStatus::kCorrupt;
        switch (type) {
            case 0:
                status = InflateStored(reader, max_output, out);
                break;
            case 1:
                status = InflateFixed(reader, max_output, out);
                break;
            case 2:
                status = InflateDynamic(reader, max_output, out);
                break;
            default:
                return InflateStatus::kCorrupt;
        }
        if (status != InflateStatus::kOk) {
            return status;
        }
    }
    return InflateStatus::kOk;
}

uint32_t Crc32(const void* data, size_t length, uint32_t crc)
{
    static const std::array<uint32_t, 256> kTable = [] {
        std::array<uint32_t, 256> table = {};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            }
            table[i] = c;
        }
        return table;
    }();
    const auto* bytes = static_cast<const uint8_t*>(data);
    crc = ~crc;
    for (size_t i = 0; i < length; ++i) {
        crc = kTable[(crc ^ bytes[i]) & 0xFFu] ^ (crc >> 8);
    }
    return ~crc;
}

}  // namespace epub
