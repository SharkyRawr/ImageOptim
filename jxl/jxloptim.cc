#include <jxl/decode.h>
#include <jxl/encode.h>
#include <jxl/decode_cxx.h>
#include <jxl/encode_cxx.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

using Bytes = std::vector<uint8_t>;
constexpr size_t limit = 512 * 1024 * 1024;
struct Image {
    JxlBasicInfo info{};
    JxlExtraChannelInfo alpha{};
    JxlColorEncoding color{};
    bool encodedColor = false;
    Bytes icc, pixels, boxes;
    JxlPixelFormat format{};
};
struct Unsupported : std::runtime_error { using std::runtime_error::runtime_error; };
static void require(bool ok) { if (!ok) throw std::runtime_error("Invalid JPEG XL or codec failure"); }
static uint64_t bigEndian(const uint8_t *p, size_t n) {
    uint64_t value = 0;
    while (n--) value = (value << 8) | *p++;
    return value;
}

// Retain metadata boxes byte-for-byte, including their Brotli representation.
// Unknown boxes may depend on the original codestream, so leave those files alone.
static Bytes metadata(const Bytes &input) {
    Bytes result;
    if (input.size() >= 2 && input[0] == 0xff && input[1] == 0x0a) return result;
    size_t pos = 0;
    while (pos < input.size()) {
        require(input.size() - pos >= 8);
        const uint8_t *p = input.data() + pos;
        uint64_t size = bigEndian(p, 4);
        size_t header = 8;
        if (size == 1) {
            require(input.size() - pos >= 16);
            size = bigEndian(p + 8, 8);
            header = 16;
        } else if (!size) size = input.size() - pos;
        require(size >= header && size <= input.size() - pos);
        const uint8_t *type = p + 4;
        bool meta = !memcmp(type, "Exif", 4) || !memcmp(type, "xml ", 4);
        if (!memcmp(type, "brob", 4)) {
            require(size >= header + 4);
            meta = !memcmp(p + header, "Exif", 4) || !memcmp(p + header, "xml ", 4);
            if (!meta) throw Unsupported("unsupported compressed metadata");
        }
        if (meta) {
            // A size-to-EOF metadata box cannot be moved ahead of other boxes.
            if (!bigEndian(p, 4)) throw Unsupported("unbounded metadata box");
            result.insert(result.end(), p, p + size);
        } else if (memcmp(type, "JXL ", 4) && memcmp(type, "ftyp", 4) &&
                   memcmp(type, "jxlc", 4) && memcmp(type, "jxlp", 4) && memcmp(type, "jxll", 4)) {
            throw Unsupported("unsupported container box: " + std::string(reinterpret_cast<const char *>(type), 4));
        }
        pos += size;
    }
    return result;
}

static Image decode(const Bytes &input) {
    Image image;
    image.boxes = metadata(input);
    auto dec = JxlDecoderMake(nullptr);
    require(bool(dec));
    require(JxlDecoderSetKeepOrientation(dec.get(), JXL_TRUE) == JXL_DEC_SUCCESS);
    require(JxlDecoderSetCoalescing(dec.get(), JXL_FALSE) == JXL_DEC_SUCCESS);
    require(JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING |
                                      JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE | JXL_DEC_BOX) == JXL_DEC_SUCCESS);
    require(JxlDecoderSetInput(dec.get(), input.data(), input.size()) == JXL_DEC_SUCCESS);
    JxlDecoderCloseInput(dec.get());
    unsigned frames = 0, complete = 0;
    for (;;) {
        auto status = JxlDecoderProcessInput(dec.get());
        if (status == JXL_DEC_BASIC_INFO) {
            auto &i = image.info;
            require(JxlDecoderGetBasicInfo(dec.get(), &i) == JXL_DEC_SUCCESS);
            if (!i.uses_original_profile || i.have_animation || i.have_preview ||
                i.exponent_bits_per_sample || (i.bits_per_sample != 8 && i.bits_per_sample != 16) ||
                i.alpha_exponent_bits || (i.alpha_bits && i.alpha_bits != i.bits_per_sample) ||
                i.num_extra_channels != unsigned(i.alpha_bits != 0) ||
                (i.num_color_channels != 1 && i.num_color_channels != 3) ||
                uint64_t(i.xsize) * i.ysize > 64000000) throw Unsupported("unsupported image features");
            if (i.alpha_bits) {
                require(JxlDecoderGetExtraChannelInfo(dec.get(), 0, &image.alpha) == JXL_DEC_SUCCESS);
                if (image.alpha.type != JXL_CHANNEL_ALPHA || image.alpha.dim_shift || image.alpha.name_length)
                    throw Unsupported("unsupported alpha channel");
            }
            image.format = {i.num_color_channels + unsigned(i.alpha_bits != 0),
                            i.bits_per_sample == 8 ? JXL_TYPE_UINT8 : JXL_TYPE_UINT16, JXL_NATIVE_ENDIAN, 0};
        } else if (status == JXL_DEC_COLOR_ENCODING) {
            image.encodedColor = JxlDecoderGetColorAsEncodedProfile(dec.get(), JXL_COLOR_PROFILE_TARGET_ORIGINAL,
                                                                   &image.color) == JXL_DEC_SUCCESS;
            size_t size;
            require(JxlDecoderGetICCProfileSize(dec.get(), JXL_COLOR_PROFILE_TARGET_ORIGINAL, &size) == JXL_DEC_SUCCESS);
            require(size <= limit);
            image.icc.resize(size);
            require(JxlDecoderGetColorAsICCProfile(dec.get(), JXL_COLOR_PROFILE_TARGET_ORIGINAL,
                                                   image.icc.data(), size) == JXL_DEC_SUCCESS);
        } else if (status == JXL_DEC_FRAME) {
            JxlFrameHeader frame{};
            require(JxlDecoderGetFrameHeader(dec.get(), &frame) == JXL_DEC_SUCCESS);
            if (++frames != 1 || !frame.is_last || frame.name_length || frame.layer_info.have_crop ||
                frame.layer_info.blend_info.blendmode != JXL_BLEND_REPLACE) throw Unsupported("unsupported frame features");
        } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            size_t size;
            require(JxlDecoderImageOutBufferSize(dec.get(), &image.format, &size) == JXL_DEC_SUCCESS);
            require(size <= limit);
            image.pixels.resize(size);
            require(JxlDecoderSetImageOutBuffer(dec.get(), &image.format, image.pixels.data(), size) == JXL_DEC_SUCCESS);
        } else if (status == JXL_DEC_FULL_IMAGE) {
            ++complete;
        } else if (status == JXL_DEC_BOX) {
            // Keep decoding through trailing boxes; metadata() retained their bytes.
            continue;
        } else if (status == JXL_DEC_SUCCESS) {
            require(JxlDecoderReleaseInput(dec.get()) == 0);
            require(frames == 1 && complete == 1 && !image.pixels.empty() && !image.icc.empty());
            return image;
        } else require(false);
    }
}

static Bytes encode(const Image &image, int effort = 9) {
    auto enc = JxlEncoderMake(nullptr);
    require(bool(enc));
    require(JxlEncoderSetBasicInfo(enc.get(), &image.info) == JXL_ENC_SUCCESS);
    require(JxlEncoderUseContainer(enc.get(), JXL_TRUE) == JXL_ENC_SUCCESS);
    if (image.info.alpha_bits)
        require(JxlEncoderSetExtraChannelInfo(enc.get(), 0, &image.alpha) == JXL_ENC_SUCCESS);
    if (image.encodedColor)
        require(JxlEncoderSetColorEncoding(enc.get(), &image.color) == JXL_ENC_SUCCESS);
    else
        require(JxlEncoderSetICCProfile(enc.get(), image.icc.data(), image.icc.size()) == JXL_ENC_SUCCESS);
    auto frame = JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
    require(frame && JxlEncoderSetFrameLossless(frame, JXL_TRUE) == JXL_ENC_SUCCESS);
    require(JxlEncoderFrameSettingsSetOption(frame, JXL_ENC_FRAME_SETTING_EFFORT, effort) == JXL_ENC_SUCCESS);
    // Preserve RGB values even where alpha is zero.
    require(JxlEncoderFrameSettingsSetOption(frame, JXL_ENC_FRAME_SETTING_KEEP_INVISIBLE, 1) == JXL_ENC_SUCCESS);
    require(JxlEncoderAddImageFrame(frame, &image.format, image.pixels.data(), image.pixels.size()) == JXL_ENC_SUCCESS);
    JxlEncoderCloseInput(enc.get());
    Bytes output(16384);
    size_t used = 0;
    for (;;) {
        uint8_t *next = output.data() + used;
        size_t available = output.size() - used;
        auto status = JxlEncoderProcessOutput(enc.get(), &next, &available);
        used = next - output.data();
        if (status == JXL_ENC_SUCCESS) break;
        require(status == JXL_ENC_NEED_MORE_OUTPUT && output.size() < limit / 2);
        output.resize(output.size() * 2);
    }
    output.resize(used);
    output.insert(output.end(), image.boxes.begin(), image.boxes.end());
    return output;
}

static bool sameImage(const Image &a, const Image &b) {
    const auto &x = a.info, &y = b.info;
    return x.xsize == y.xsize && x.ysize == y.ysize && x.bits_per_sample == y.bits_per_sample &&
        x.num_color_channels == y.num_color_channels && x.alpha_bits == y.alpha_bits &&
        x.alpha_premultiplied == y.alpha_premultiplied && x.orientation == y.orientation &&
        x.intensity_target == y.intensity_target && x.min_nits == y.min_nits &&
        x.relative_to_max_display == y.relative_to_max_display && x.linear_below == y.linear_below &&
        x.intrinsic_xsize == y.intrinsic_xsize && x.intrinsic_ysize == y.intrinsic_ysize &&
        a.icc == b.icc && a.pixels == b.pixels && a.boxes == b.boxes;
}

int main(int argc, char **argv) {
    if (argc != 3) return 1;
    try {
        std::ifstream file(argv[1], std::ios::binary | std::ios::ate);
        require(bool(file));
        auto size = file.tellg();
        require(size > 0 && size <= std::streamoff(limit));
        Bytes input(static_cast<size_t>(size));
        file.seekg(0);
        require(bool(file.read(reinterpret_cast<char *>(input.data()), input.size())));
        Image original = decode(input);
        Bytes output = encode(original);
        require(sameImage(original, decode(output)));
        if (output.size() >= input.size()) return 2;
        std::ofstream dest(argv[2], std::ios::binary);
        dest.write(reinterpret_cast<const char *>(output.data()), output.size());
        dest.close();
        require(bool(dest));
        return 0;
    } catch (const Unsupported &) {
        return 2;
    } catch (const std::exception &e) {
        fprintf(stderr, "jxloptim: %s\n", e.what());
        return 1;
    }
}
