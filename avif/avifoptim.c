#include <avif/avif.h>
#include <stdio.h>
#include <string.h>

static int sameData(avifRWData a, avifRWData b) {
    return a.size == b.size && (!a.size || !memcmp(a.data, b.data, a.size));
}

// Compare decoded samples, not RGB conversions (which can hide rounding or depth loss).
static int sameImage(const avifImage *a, const avifImage *b) {
    if (a->width != b->width || a->height != b->height || a->depth != b->depth ||
        a->yuvFormat != b->yuvFormat || a->yuvRange != b->yuvRange ||
        a->yuvChromaSamplePosition != b->yuvChromaSamplePosition ||
        a->alphaPremultiplied != b->alphaPremultiplied ||
        a->colorPrimaries != b->colorPrimaries ||
        a->transferCharacteristics != b->transferCharacteristics ||
        a->matrixCoefficients != b->matrixCoefficients ||
        memcmp(&a->clli, &b->clli, sizeof(a->clli)) ||
        a->transformFlags != b->transformFlags ||
        ((a->transformFlags & AVIF_TRANSFORM_PASP) && memcmp(&a->pasp, &b->pasp, sizeof(a->pasp))) ||
        ((a->transformFlags & AVIF_TRANSFORM_CLAP) && memcmp(&a->clap, &b->clap, sizeof(a->clap))) ||
        ((a->transformFlags & AVIF_TRANSFORM_IROT) && a->irot.angle != b->irot.angle) ||
        ((a->transformFlags & AVIF_TRANSFORM_IMIR) && a->imir.axis != b->imir.axis) ||
        !sameData(a->icc, b->icc) || !sameData(a->exif, b->exif) || !sameData(a->xmp, b->xmp) ||
        a->numProperties != b->numProperties) return 0;
    for (size_t i = 0; i < a->numProperties; ++i) {
        if (memcmp(a->properties[i].boxtype, b->properties[i].boxtype, 4) ||
            memcmp(a->properties[i].usertype, b->properties[i].usertype, 16) ||
            !sameData(a->properties[i].boxPayload, b->properties[i].boxPayload)) return 0;
    }
    for (int plane = 0; plane < 4; ++plane) {
        const uint8_t *ap = avifImagePlane(a, plane), *bp = avifImagePlane(b, plane);
        if (!!ap != !!bp) return 0;
        if (!ap) continue;
        size_t bytes = avifImagePlaneWidth(a, plane) * (a->depth > 8 ? 2 : 1);
        for (uint32_t y = 0; y < avifImagePlaneHeight(a, plane); ++y) {
            if (memcmp(ap + y * avifImagePlaneRowBytes(a, plane),
                       bp + y * avifImagePlaneRowBytes(b, plane), bytes)) return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: avifoptim input.avif output.avif\n");
        return 1;
    }
    int status = 1;
    avifResult result = AVIF_RESULT_OUT_OF_MEMORY;
    avifDecoder *decoder = avifDecoderCreate();
    avifDecoder *check = avifDecoderCreate();
    avifEncoder *encoder = avifEncoderCreate();
    avifRWData output = AVIF_DATA_EMPTY;
    if (!decoder || !check || !encoder) goto done;
    decoder->imageContentToDecode = AVIF_IMAGE_CONTENT_ALL;
    result = avifDecoderSetIOFile(decoder, argv[1]);
    if (result != AVIF_RESULT_OK) goto done;
    result = avifDecoderParse(decoder);
    if (result != AVIF_RESULT_OK) goto done;
    // ponytail: still images only; preserve these richer formats by skipping until explicitly supported.
    if (decoder->imageCount != 1 || decoder->imageSequenceTrackPresent ||
        decoder->progressiveState != AVIF_PROGRESSIVE_STATE_UNAVAILABLE || decoder->image->gainMap) {
        fprintf(stderr, "Animated, progressive, or gain-map AVIF is unsupported\n");
        status = 2;
        goto done;
    }
    result = avifDecoderNextImage(decoder);
    if (result != AVIF_RESULT_OK) goto done;
    encoder->codecChoice = AVIF_CODEC_CHOICE_AOM;
    encoder->maxThreads = 1; // JobQueue already parallelizes files.
    encoder->speed = 4;
    encoder->quality = AVIF_QUALITY_LOSSLESS;
    encoder->qualityAlpha = AVIF_QUALITY_LOSSLESS;
    result = avifEncoderWrite(encoder, decoder->image, &output);
    if (result != AVIF_RESULT_OK) goto done;
    result = avifDecoderSetIOMemory(check, output.data, output.size);
    if (result != AVIF_RESULT_OK) goto done;
    result = avifDecoderParse(check);
    if (result != AVIF_RESULT_OK) goto done;
    result = avifDecoderNextImage(check);
    if (result != AVIF_RESULT_OK) goto done;
    if (!sameImage(decoder->image, check->image)) {
        fprintf(stderr, "Re-encoded AVIF changed samples or metadata; keeping original\n");
        status = 2;
        goto done;
    }
    result = AVIF_RESULT_IO_ERROR;
    FILE *file = fopen(argv[2], "wb");
    if (!file) goto done;
    int written = fwrite(output.data, 1, output.size, file) == output.size;
    int closed = fclose(file) == 0;
    if (written && closed) status = 0;
done:
    if (status == 1) fprintf(stderr, "AVIF optimization failed: %s\n", avifResultToString(result));
    avifRWDataFree(&output);
    if (encoder) avifEncoderDestroy(encoder);
    if (check) avifDecoderDestroy(check);
    if (decoder) avifDecoderDestroy(decoder);
    return status;
}
