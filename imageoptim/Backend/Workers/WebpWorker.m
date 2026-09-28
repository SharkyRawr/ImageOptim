#import "WebpWorker.h"
#import "../Job.h"
#import "../TempFile.h"

@implementation WebpWorker

+ (NSString *)encoderPath {
    NSBundle *bundle = [NSBundle bundleForClass:self];
    NSString *path = [bundle pathForAuxiliaryExecutable:@"cwebp"] ?: [bundle pathForResource:@"cwebp" ofType:@""];
    if (path && [[NSFileManager defaultManager] isExecutableFileAtPath:path]) return path;
    return nil;
}

- (BOOL)optimizeFile:(File *)file toTempPath:(NSURL *)temp {
    NSString *path = [WebpWorker encoderPath];
    if (!path) {
        [job setError:@"Bundled WebP encoder (cwebp) is unavailable"];
        return NO;
    }

    // ponytail: cwebp handles still WebP only; add an animation encoder if animations become necessary.
    [self taskWithPath:path arguments:@[@"-lossless", @"-m", @"6", @"-exact",
                                       @"-metadata", @"all", @"-quiet",
                                       file.path.path, @"-o", temp.path]];
    NSFileHandle *devnull = [NSFileHandle fileHandleWithNullDevice];
    task.standardInput = devnull;
    task.standardOutput = devnull;
    task.standardError = devnull;
    [self launchTask];
    if (![self waitUntilTaskExit]) return NO;

    return [job setFileOptimized:[file tempCopyOfPath:temp] toolName:@"cwebp"];
}

@end
