#define QOI_IMPLEMENTATION
#define QOI_NO_STDIO
#include "vendor/qoi.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

using Bytes = std::vector<uint8_t>;
constexpr size_t limit = 512 * 1024 * 1024;
struct Unsupported : std::runtime_error { using std::runtime_error::runtime_error; };
static void require(bool ok) { if (!ok) throw std::runtime_error("Invalid QOI or codec/I/O failure"); }
static uint32_t bigEndian(const uint8_t *p) {
    return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}
struct Image { qoi_desc info{}; Bytes pixels; };

static Image decode(const Bytes &input) {
    require(input.size() >= 22 && input.size() <= limit && !memcmp(input.data(), "qoif", 4));
    uint32_t width = bigEndian(input.data() + 4), height = bigEndian(input.data() + 8);
    require(width && height && (input[12] == 3 || input[12] == 4) && input[13] <= 1);
    uint64_t pixels = uint64_t(width) * height;
    if (pixels > 64000000) throw Unsupported("image exceeds 64 million pixels");
    size_t end = input.size() - 8, pos = 14;
    require(!memcmp(input.data() + end, qoi_padding, 8));
    // The reference decoder tolerates missing chunks and reads past truncated operands.
    for (uint64_t produced = 0; produced < pixels;) {
        require(pos < end);
        uint8_t op = input[pos++];
        size_t operands = op == 0xfe ? 3 : op == 0xff ? 4 : (op & 0xc0) == 0x80 ? 1 : 0;
        require(operands <= end - pos);
        pos += operands;
        uint64_t count = op < 0xfe && (op & 0xc0) == 0xc0 ? (op & 0x3f) + 1 : 1;
        require(count <= pixels - produced);
        produced += count;
    }
    require(pos == end);
    Image image;
    std::unique_ptr<void, decltype(&free)> decoded(qoi_decode(input.data(), int(input.size()), &image.info, 4), free);
    require(bool(decoded));
    auto *data = static_cast<const uint8_t *>(decoded.get());
    image.pixels.assign(data, data + pixels * 4);
    return image;
}

static Bytes encode(const Image &image) {
    qoi_desc info = image.info;
    // Channels are informational in QOI; retain RGBA samples even in RGB-tagged streams.
    info.channels = 4;
    int size = 0;
    std::unique_ptr<void, decltype(&free)> encoded(qoi_encode(image.pixels.data(), &info, &size), free);
    require(bool(encoded) && size > 0 && size_t(size) <= limit);
    auto *data = static_cast<const uint8_t *>(encoded.get());
    Bytes output(data, data + size);
    output[12] = image.info.channels;
    return output;
}
static bool sameImage(const Image &a, const Image &b) {
    return a.info.width == b.info.width && a.info.height == b.info.height &&
        a.info.channels == b.info.channels && a.info.colorspace == b.info.colorspace && a.pixels == b.pixels;
}
static Bytes readFile(const char *path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    require(bool(file));
    auto size = file.tellg();
    require(size >= 0);
    if (size > std::streamoff(limit)) throw Unsupported("file exceeds 512 MiB");
    Bytes input(static_cast<size_t>(size));
    file.seekg(0);
    require(bool(file.read(reinterpret_cast<char *>(input.data()), input.size())));
    return input;
}
static void writeFile(const char *path, const Bytes &bytes) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    file.close();
    require(bool(file));
}
int main(int argc, char **argv) {
    if (argc != 3) return 1;
    try {
        Bytes input = readFile(argv[1]);
        Image original = decode(input);
        Bytes output = encode(original);
        require(sameImage(original, decode(output)));
        if (output.size() >= input.size()) return 2;
        writeFile(argv[2], output);
        return 0;
    } catch (const Unsupported &) {
        return 2;
    } catch (const std::exception &e) {
        fprintf(stderr, "qoioptim: %s\n", e.what());
        return 1;
    }
}
