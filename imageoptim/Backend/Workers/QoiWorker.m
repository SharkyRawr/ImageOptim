#import "QoiWorker.h"
#import "../Job.h"
#import "../TempFile.h"

@implementation QoiWorker

+ (NSString *)encoderPath {
    NSBundle *bundle = [NSBundle bundleForClass:self];
    NSString *path = [bundle pathForAuxiliaryExecutable:@"qoioptim"] ?: [bundle pathForResource:@"qoioptim" ofType:@""];
    return path && [[NSFileManager defaultManager] isExecutableFileAtPath:path] ? path : nil;
}

- (BOOL)optimizeFile:(File *)file toTempPath:(NSURL *)temp {
    NSString *path = [QoiWorker encoderPath];
    if (!path) {
        [job setError:@"Bundled QOI optimizer is unavailable"];
        return NO;
    }
    [self taskWithPath:path arguments:@[file.path.path, temp.path]];
    task.standardInput = [NSFileHandle fileHandleWithNullDevice];
    task.standardOutput = [NSFileHandle fileHandleWithNullDevice];
    [self launchTask];
    if (![self waitUntilTaskExit]) {
        // Exit 2 means the image is unsupported or cannot be made smaller.
        if (!self.isCancelled && task.terminationStatus != 2) [job setError:@"QOI optimization failed"];
        return NO;
    }
    return [job setFileOptimized:[file tempCopyOfPath:temp] toolName:@"qoioptim"];
}

@end
