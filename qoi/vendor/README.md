Official QOI reference codec from https://github.com/phoboslab/qoi
Pinned commit: ffb2d2cb74a1de60819b21b939f7209aa53e91c1

qoi.h and LICENSE are unmodified upstream files. The codec is MIT-licensed.
The optimizer validates chunk bounds, pixel counts, and the end marker before
calling the reference decoder, and verifies decoded samples after encoding.
