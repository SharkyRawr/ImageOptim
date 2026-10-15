#define main optimizeMain
#include "qoioptim.cc"
#undef main
#include <cassert>
#include <unistd.h>

static Bytes verbose(const Image &image) {
    Bytes result{'q','o','i','f'};
    for (uint32_t n : {image.info.width, image.info.height})
        for (int shift : {24, 16, 8, 0}) result.push_back(n >> shift);
    result.push_back(image.info.channels);
    result.push_back(image.info.colorspace);
    for (size_t p = 0; p < image.pixels.size(); p += 4) {
        result.push_back(0xff);
        result.insert(result.end(), image.pixels.begin() + p, image.pixels.begin() + p + 4);
    }
    result.insert(result.end(), std::begin(qoi_padding), std::end(qoi_padding));
    return result;
}
int main(int argc, char **argv) {
    char directory[] = "/tmp/imageoptim-qoi-XXXXXX";
    assert(mkdtemp(directory));
    std::string input = std::string(directory) + "/input.qoi", output = std::string(directory) + "/output.qoi";
    char *args[] = {const_cast<char *>("qoioptim"), input.data(), output.data()};
    Bytes sentinel{42};
    for (unsigned char channels : {3, 4}) for (unsigned char colorspace : {0, 1}) {
        Image image;
        image.info = {32, 16, channels, colorspace};
        for (size_t p = 0; p < 512; ++p) {
            // Include transparent color and nonopaque samples even when channels is informationally RGB.
            image.pixels.insert(image.pixels.end(), {uint8_t(p / 32), uint8_t(p / 32 + 19), 127,
                                                     uint8_t(p < 256 ? 0 : 255)});
        }
        Bytes original = verbose(image), canonical = encode(image);
        assert(canonical.size() < original.size() && sameImage(image, decode(original)));
        assert(sameImage(image, decode(canonical)));
        writeFile(input.c_str(), original);
        assert(optimizeMain(3, args) == 0);
        assert(readFile(output.c_str()) == canonical);
        writeFile(input.c_str(), canonical);
        writeFile(output.c_str(), sentinel);
        assert(optimizeMain(3, args) == 2 && readFile(output.c_str()) == sentinel);
        if (channels == 3 && colorspace == 0 && argc == 2) writeFile(argv[1], original);
    }
    // Exercise a larger image with deterministic pseudorandom samples.
    uint32_t seed = 42;
    Image random;
    random.info = {1000, 1, 4, 1};
    for (unsigned i = 0; i < 4000; ++i) {
        seed = seed * 1664525 + 1013904223;
        random.pixels.push_back(seed >> 24);
    }
    assert(sameImage(random, decode(encode(random))));
    // Explicit RGB, RGBA, INDEX, DIFF, LUMA, and RUN chunks with known decoded samples.
    Image chunksImage;
    chunksImage.info = {7, 1, 4, 0};
    chunksImage.pixels = {10,20,30,255, 10,20,30,0, 10,20,30,255,
                          11,21,31,255, 12,21,31,255, 12,21,31,255, 12,21,31,255};
    Bytes chunked = verbose(chunksImage);
    chunked.resize(14);
    chunked.insert(chunked.end(), {0xfe,10,20,30, 0xff,10,20,30,0, 9, 0x7f, 0xa0,0x98, 0xc1});
    chunked.insert(chunked.end(), std::begin(qoi_padding), std::end(qoi_padding));
    assert(sameImage(chunksImage, decode(chunked)));
    assert(sameImage(chunksImage, decode(encode(decode(chunked)))));
    Image one;
    one.info = {1, 1, 3, 0};
    one.pixels = {12, 34, 56, 255};
    Bytes valid = verbose(one);
    std::vector<Bytes> invalid{Bytes{}, Bytes{'q','o','i','f'}};
    for (size_t length = 0; length < valid.size(); ++length)
        invalid.emplace_back(valid.begin(), valid.begin() + length);
    for (size_t field : {size_t(0), size_t(12), size_t(13), valid.size() - 1}) {
        Bytes damaged = valid;
        damaged[field] = 42;
        invalid.push_back(damaged);
    }
    Bytes zero = valid;
    zero[7] = 0;
    invalid.push_back(zero);
    // A run past the declared pixel count; extra chunk; missing operands before a valid marker.
    for (Bytes chunks : {Bytes{0xc1}, Bytes{0xc0, 0xc0}, Bytes{0xff, 1, 2, 3}, Bytes{0xfe, 1, 2}, Bytes{0x80}}) {
        Bytes damaged(valid.begin(), valid.begin() + 14);
        damaged.insert(damaged.end(), chunks.begin(), chunks.end());
        damaged.insert(damaged.end(), std::begin(qoi_padding), std::end(qoi_padding));
        invalid.push_back(damaged);
    }
    for (const auto &bytes : invalid) {
        writeFile(input.c_str(), bytes);
        writeFile(output.c_str(), sentinel);
        assert(optimizeMain(3, args) == 1 && readFile(output.c_str()) == sentinel);
    }
    Bytes oversized = valid;
    oversized[4] = 4;
    writeFile(input.c_str(), oversized);
    assert(optimizeMain(3, args) == 2 && readFile(output.c_str()) == sentinel);
    assert(optimizeMain(1, args) == 1);
    remove(input.c_str());
    remove(output.c_str());
    rmdir(directory);
}
