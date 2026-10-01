#include <cassert>
#define main optimizeMain
#include "jxloptim.cc"
#undef main

static void writeFile(const char *path, const Bytes &bytes) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    file.close();
    assert(file);
}
static Bytes readFile(const char *path) {
    std::ifstream file(path, std::ios::binary);
    return Bytes(std::istreambuf_iterator<char>(file), {});
}
int main(int argc, char **argv) {
    char input[] = "test-input.jxl", output[] = "test-output.jxl", tool[] = "jxloptim";
    char *args[] = {tool, input, output};
    bool smaller = false;
    for (unsigned bits : {8u, 16u}) for (unsigned channels : {1u, 3u}) for (bool alpha : {false, true}) {
        Image image;
        JxlEncoderInitBasicInfo(&image.info);
        auto &i = image.info;
        i.xsize = i.ysize = 64;
        i.bits_per_sample = bits;
        i.num_color_channels = channels;
        i.uses_original_profile = JXL_TRUE;
        i.orientation = JXL_ORIENT_ROTATE_90_CW;
        i.num_extra_channels = alpha;
        i.alpha_bits = alpha ? bits : 0;
        i.alpha_premultiplied = alpha;
        JxlEncoderInitExtraChannelInfo(JXL_CHANNEL_ALPHA, &image.alpha);
        image.alpha.bits_per_sample = bits;
        image.alpha.alpha_premultiplied = alpha;
        JxlColorEncodingSetToSRGB(&image.color, channels == 1);
        image.encodedColor = true;
        image.format = {channels + unsigned(alpha), bits == 8 ? JXL_TYPE_UINT8 : JXL_TYPE_UINT16, JXL_NATIVE_ENDIAN, 0};
        image.pixels.resize(i.xsize * i.ysize * image.format.num_channels * bits / 8);
        for (size_t p = 0; p < image.pixels.size(); ++p) image.pixels[p] = (p / 64 + p % 7) % 256;
        // Nonzero RGB under fully transparent pixels must also survive.
        if (alpha) {
            size_t stride = image.format.num_channels * bits / 8;
            for (size_t p = 0; p < image.pixels.size(); p += stride)
                memset(image.pixels.data() + p + stride - bits / 8, 0, bits / 8);
        }
        image.boxes = {0,0,0,12,'x','m','l',' ','<','x','/','>',
                       0,0,0,12,'E','x','i','f',0,0,0,0};
        Bytes original = encode(image, 1);
        Image decoded = decode(original);
        assert(decoded.pixels == image.pixels && decoded.boxes == image.boxes);
        if (channels == 3) {
            // Exercise the ICC-profile encoder path as well as structured color.
            image.encodedColor = false;
            image.icc = decoded.icc;
            assert(sameImage(decoded, decode(encode(image))));
        }
        // Raw codestreams and container files share the same worker path.
        Bytes raw;
        for (size_t pos = 0; pos < original.size();) {
            size_t size = bigEndian(original.data() + pos, 4);
            assert(size >= 8 && size <= original.size() - pos);
            const uint8_t *type = original.data() + pos + 4;
            if (!memcmp(type, "jxlc", 4)) raw.insert(raw.end(), original.begin() + pos + 8, original.begin() + pos + size);
            if (!memcmp(type, "jxlp", 4)) raw.insert(raw.end(), original.begin() + pos + 12, original.begin() + pos + size);
            pos += size;
        }
        assert(!raw.empty() && decode(raw).pixels == decoded.pixels);
        Bytes candidate = encode(decoded);
        assert(sameImage(decoded, decode(candidate)));
        Image changed = decode(candidate);
        changed.pixels[0] ^= 1;
        assert(!sameImage(decoded, changed));
        writeFile(input, original);
        int result = optimizeMain(3, args);
        assert(result == (candidate.size() < original.size() ? 0 : 2));
        if (result == 0) {
            assert(sameImage(decoded, decode(readFile(output))));
            if (!smaller && argc == 2) writeFile(argv[1], original);
            smaller = true;
        }
        // An already-optimized file never produces a replacement.
        writeFile(input, candidate);
        writeFile(output, Bytes{42});
        assert(optimizeMain(3, args) == 2 && readFile(output) == Bytes{42});
        // Reconstruction and unknown boxes must never be dropped.
        original.insert(original.end(), {0,0,0,8,'j','b','r','d'});
        writeFile(input, original);
        assert(optimizeMain(3, args) == 2 && readFile(output) == Bytes{42});
        i.have_animation = JXL_TRUE;
        i.animation.tps_numerator = 10;
        i.animation.tps_denominator = 1;
        writeFile(input, encode(image, 1));
        assert(optimizeMain(3, args) == 2);
    }
    assert(smaller);
    for (Bytes invalid : {Bytes{}, Bytes{0xff, 0x0a}, Bytes{0,0,0,1,'J','X','L',' '}}) {
        writeFile(input, invalid);
        assert(optimizeMain(3, args) == 1);
    }
    remove(input);
    remove(output);
}
