//
//  File.m
//  ImageOptim
//
//  Created by Kornel on 11/01/2017.
//
//

#import "File.h"
#import "TempFile.h"
#import "../log.h"
#import <assert.h>

@implementation File

- (nullable instancetype)initWithType:(enum IOFileType)type size:(NSUInteger)size fromPath:(NSURL *)aPath {
    if (!size) {
        return nil;
    }

    if ((self = [super init])) {
        _path = aPath;
        _byteSize = size;
        fileType = type;
    }
    return self;
}

-(instancetype)initWithData:(NSData *)fileData fromPath:(NSURL *)aPath {
    const unsigned char pngheader[] = {0x89,0x50,0x4e,0x47,0x0d,0x0a};
    const unsigned char jpegheader[] = {0xff,0xd8,0xff};
    const unsigned char gifheader[] = {0x47,0x49,0x46,0x38};
    const unsigned char svgheader[] = {'<','s','v','g'};
    const unsigned char riffheader[] = {'R','I','F','F'};
    const unsigned char webpheader[] = {'W','E','B','P'};
    char fileHeaderBytes[12];

    if (!fileData || fileData.length < 6) {
        return nil;
    }

    [fileData getBytes:fileHeaderBytes length:MIN(fileData.length, sizeof(fileHeaderBytes))];

    enum IOFileType type = 0;

    if (0 == memcmp(fileHeaderBytes, pngheader, sizeof(pngheader))) {
        type = FILETYPE_PNG;
    } else if (0 == memcmp(fileHeaderBytes, jpegheader, sizeof(jpegheader))) {
        type = FILETYPE_JPEG;
    } else if (0 == memcmp(fileHeaderBytes, gifheader, sizeof(gifheader))) {
        type = FILETYPE_GIF;
    } else if (0 == memcmp(fileHeaderBytes, svgheader, sizeof(svgheader)) || [aPath.pathExtension isEqualToString:@"svg"]) {
        type = FILETYPE_SVG;
    } else if (fileData.length >= sizeof(fileHeaderBytes) &&
               0 == memcmp(fileHeaderBytes, riffheader, sizeof(riffheader)) &&
               0 == memcmp(fileHeaderBytes + 8, webpheader, sizeof(webpheader))) {
        type = FILETYPE_WEBP;
    }

    if (!type && fileData.length >= 16) {
        const unsigned char *bytes = fileData.bytes;
        uint32_t boxSize = ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
                           ((uint32_t)bytes[2] << 8) | bytes[3];
        if (!memcmp(bytes + 4, "ftyp", 4) && boxSize >= 16 &&
            boxSize <= fileData.length && boxSize % 4 == 0) {
            // Check the major and compatible brands, excluding the minor-version field.
            for (NSUInteger offset = 8; offset + 4 <= boxSize; offset += 4) {
                if (offset == 12) continue;
                if (!memcmp(bytes + offset, "avif", 4) || !memcmp(bytes + offset, "avis", 4)) {
                    type = FILETYPE_AVIF;
                    break;
                }
            }
        }
    }

    return [self initWithType:type size:fileData.length fromPath:aPath];
}

- (nullable File *)copyOfPath:(NSURL *)path {
    return [[File alloc] initWithType:fileType size:[File byteSize:path] fromPath:path];
}

- (nullable File *)copyOfPath:(NSURL *)path size:(NSUInteger)s {
    return [[File alloc] initWithType:fileType size:s fromPath:path];
}

- (nullable TempFile *)tempCopyOfPath:(NSURL *)path {
    return [[TempFile alloc] initWithType:fileType size:[File byteSize:path] fromPath:path];
}

- (nullable TempFile *)tempCopyOfPath:(NSURL *)path size:(NSUInteger)s {
    if (!s) {
        return nil;
    }

    if (s != [File byteSize:path]) {
        NSLog(@"Expected size %d, but file is actually %d", (int)s, (int)[File byteSize:path]);
        return nil;
    }
    return [[TempFile alloc] initWithType:fileType size:s fromPath:path];
}

- (BOOL)isLarge {
    if (fileType == FILETYPE_PNG) {
        return _byteSize > 250 * 1024;
    }
    return _byteSize > 1 * 1024 * 1024;
}

- (BOOL)isSmall {
    if (fileType == FILETYPE_PNG) {
        return _byteSize < 2048;
    }
    return _byteSize < 10 * 1024;
}

+ (NSInteger)byteSize:(NSURL *)afile {
    NSNumber *value = nil;
    NSError *err = nil;
    if ([afile getResourceValue:&value forKey:NSURLFileSizeKey error:&err] && value) {
        return [value integerValue];
    }
    IOWarn("Could not stat %@: %@", afile.path, err);
    return 0;
}

- (nullable NSString *)mimeType {
    switch (fileType) {
        case FILETYPE_PNG: return @"image/png";
        case FILETYPE_JPEG: return @"image/jpeg";
        case FILETYPE_GIF: return @"image/gif";
        case FILETYPE_SVG: return @"image/svg";
        case FILETYPE_WEBP: return @"image/webp";
        case FILETYPE_AVIF: return @"image/avif";
        default:
            return nil;
    }
}

@end
