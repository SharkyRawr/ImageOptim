#include <assert.h>
int optimizeMain(int argc, char **argv);
#define main optimizeMain
#include "avifoptim.c"
#undef main

static void writeData(const char *path, avifRWData data) {
    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fwrite(data.data, 1, data.size, file) == data.size);
    assert(!fclose(file));
}

int main(void) {
    char *args[] = {"avifoptim", "test-input.avif", "test-output.avif"};
    const avifPixelFormat formats[] = {AVIF_PIXEL_FORMAT_YUV444, AVIF_PIXEL_FORMAT_YUV422,
                                      AVIF_PIXEL_FORMAT_YUV420, AVIF_PIXEL_FORMAT_YUV400};
    for (uint32_t depth = 8; depth <= 12; depth += 2) {
        for (size_t format = 0; format < 4; ++format) {
            avifImage *image = avifImageCreate(64, 64, depth, formats[format]);
            assert(image && avifImageAllocatePlanes(image, format == 1 ? AVIF_PLANES_YUV : AVIF_PLANES_ALL) == AVIF_RESULT_OK);
            image->yuvRange = format % 2 ? AVIF_RANGE_LIMITED : AVIF_RANGE_FULL;
            image->colorPrimaries = AVIF_COLOR_PRIMARIES_BT2020;
            image->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_SMPTE2084;
            image->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT2020_NCL;
            image->clli.maxCLL = 1000;
            image->clli.maxPALL = 400;
            image->alphaPremultiplied = image->alphaPlane != NULL;
            for (int plane = 0; plane < 4; ++plane) {
                uint8_t *pixels = avifImagePlane(image, plane);
                if (!pixels) continue;
                for (uint32_t y = 0; y < avifImagePlaneHeight(image, plane); ++y) {
                    for (uint32_t x = 0; x < avifImagePlaneWidth(image, plane); ++x) {
                        uint16_t sample = (((x / 8 + y / 8 + plane) * 19) % 256) << (depth - 8);
                        if (depth == 8) pixels[y * avifImagePlaneRowBytes(image, plane) + x] = sample;
                        else ((uint16_t *)(pixels + y * avifImagePlaneRowBytes(image, plane)))[x] = sample;
                    }
                }
            }
            assert(avifImageSetProfileICC(image, (const uint8_t *)"test-profile", 12) == AVIF_RESULT_OK);
            assert(avifImageSetMetadataXMP(image, (const uint8_t *)"<x:xmpmeta/>", 12) == AVIF_RESULT_OK);
            const uint8_t exif[] = {'I', 'I', 42, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            assert(avifImageSetMetadataExif(image, exif, sizeof(exif)) == AVIF_RESULT_OK);
            image->transformFlags = AVIF_TRANSFORM_IROT | AVIF_TRANSFORM_IMIR | AVIF_TRANSFORM_PASP;
            image->irot.angle = 1;
            image->imir.axis = 1;
            image->pasp.hSpacing = 2;
            image->pasp.vSpacing = 1;
            avifEncoder *encoder = avifEncoderCreate();
            assert(encoder);
            encoder->quality = encoder->qualityAlpha = AVIF_QUALITY_LOSSLESS;
            encoder->speed = 10;
            avifRWData data = AVIF_DATA_EMPTY;
            assert(avifEncoderWrite(encoder, image, &data) == AVIF_RESULT_OK);
            writeData(args[1], data);
            assert(optimizeMain(3, args) == 0);
            avifDecoder *decoded = avifDecoderCreate();
            assert(decoded && avifDecoderSetIOFile(decoded, args[2]) == AVIF_RESULT_OK);
            assert(avifDecoderParse(decoded) == AVIF_RESULT_OK);
            assert(avifDecoderNextImage(decoded) == AVIF_RESULT_OK);
            assert(sameImage(image, decoded->image));
            decoded->image->yuvPlanes[0][0] ^= 1;
            assert(!sameImage(image, decoded->image));
            avifDecoderDestroy(decoded);
            avifRWDataFree(&data);
            avifEncoderDestroy(encoder);

            avifImageDestroy(image);
        }
    }
    const char *unsupported[] = {"colors-animated-8bpc.avif", "draw_points_idat_progressive.avif", "seine_sdr_gainmap_srgb.avif"};
    for (size_t i = 0; i < 3; ++i) {
        char input[1024];
        snprintf(input, sizeof(input), "%s/%s", TEST_DATA_DIR, unsupported[i]);
        char *rejected[] = {"avifoptim", input, "test-rejected.avif"};
        assert(optimizeMain(3, rejected) == 2);
        assert(!fopen(rejected[2], "rb"));
    }
    char *missing[] = {"avifoptim", "missing.avif", "test-rejected.avif"};
    assert(optimizeMain(3, missing) == 1);
    assert(!fopen(missing[2], "rb"));
    avifRWData malformed = {(uint8_t *)"broken", 6};
    writeData("test-malformed.avif", malformed);
    char *bad[] = {"avifoptim", "test-malformed.avif", "test-rejected.avif"};
    assert(optimizeMain(3, bad) == 1);
    assert(!fopen(bad[2], "rb"));
    puts("AVIF checks passed: 8/10/12-bit YUV444/422/420/400, alpha, metadata, HDR, transforms, and animation rejection");
    return 0;
}
