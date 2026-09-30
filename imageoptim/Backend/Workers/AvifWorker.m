#import "AvifWorker.h"
#import "../Job.h"
#import "../TempFile.h"

@implementation AvifWorker

+ (NSString *)encoderPath {
    NSBundle *bundle = [NSBundle bundleForClass:self];
    NSString *path = [bundle pathForAuxiliaryExecutable:@"avifoptim"] ?: [bundle pathForResource:@"avifoptim" ofType:@""];
    return path && [[NSFileManager defaultManager] isExecutableFileAtPath:path] ? path : nil;
}

- (BOOL)optimizeFile:(File *)file toTempPath:(NSURL *)temp {
    NSString *path = [AvifWorker encoderPath];
    if (!path) {
        [job setError:@"Bundled AVIF optimizer is unavailable"];
        return NO;
    }
    [self taskWithPath:path arguments:@[file.path.path, temp.path]];
    task.standardInput = [NSFileHandle fileHandleWithNullDevice];
    task.standardOutput = [NSFileHandle fileHandleWithNullDevice];
    [self launchTask];
    if (![self waitUntilTaskExit]) {
        // Exit 2 means an unsupported image was deliberately left untouched.
        if (!self.isCancelled && task.terminationStatus != 2) [job setError:@"AVIF optimization failed"];
        return NO;
    }
    return [job setFileOptimized:[file tempCopyOfPath:temp] toolName:@"avifoptim"];
}

@end
