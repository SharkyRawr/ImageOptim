//
//  BackendTests.m
//  BackendTests
//
//  Created by Kornel on 20/04/2015.
//
//

@import Cocoa;
#import <XCTest/XCTest.h>
#import "Job.h"
#import "JobQueue.h"
#import "File.h"

@interface BackendTests : XCTestCase

@end

@implementation BackendTests

- (void)setUp {
    [super setUp];
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

- (void)testCompressOne {
    NSURL *origPath = [[NSBundle bundleForClass:[self class]] URLForResource:@"unoptimized" withExtension:@"png"];
    NSURL *path = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:[[NSUUID UUID] UUIDString]]];

    NSFileManager *fm = [NSFileManager defaultManager];
    XCTAssertTrue([fm copyItemAtURL:origPath toURL:path error:nil]);

    Job *f = [[Job alloc] initWithFilePath:path resultsDatabase:nil];
    JobQueue *q = [[JobQueue alloc] initWithCPUs:4
                                            dirs:1
                                           files:4
                                        defaults:[NSUserDefaults standardUserDefaults]];

    [q addJob:f];
    XCTAssertTrue([f isBusy]);
    XCTAssertFalse([f isDone]);
    XCTAssertFalse([f isFailed]);
    [q wait];
    XCTAssertFalse([f isBusy]);

    NSNumber *size, *origSize;
    [path removeAllCachedResourceValues];
    [origPath removeAllCachedResourceValues];

    XCTAssertTrue([path getResourceValue:&size forKey:NSURLFileSizeKey error:nil]);
    XCTAssertTrue([origPath getResourceValue:&origSize forKey:NSURLFileSizeKey error:nil]);

    XCTAssertTrue([f isDone]);
    XCTAssertFalse([f isFailed]);
    XCTAssertFalse([f isStoppable]);

    XCTAssertLessThan(1, 2);
    XCTAssertLessThan([size integerValue], [origSize integerValue]);
    XCTAssertLessThanOrEqual(1, 2);
    XCTAssertLessThanOrEqual([size integerValue], 5552);

    XCTAssertTrue([f canRevert]);

    XCTAssertEqual([[f byteSizeOptimized] integerValue], [size integerValue]);
    XCTAssertEqual([[f byteSizeOriginal] integerValue], [origSize integerValue]);
}

- (void)testCompressWebP {
    NSURL *original = [[NSBundle bundleForClass:[self class]] URLForResource:@"unoptimized" withExtension:@"webp"];
    NSURL *path = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:[NSString stringWithFormat:@"%@.webp", [NSUUID UUID].UUIDString]]];
    XCTAssertTrue([[NSFileManager defaultManager] copyItemAtURL:original toURL:path error:nil]);

    NSData *before = [NSData dataWithContentsOfURL:path];
    XCTAssertEqualObjects([[[File alloc] initWithData:before fromPath:path] mimeType], @"image/webp");

    Job *job = [[Job alloc] initWithFilePath:path resultsDatabase:nil];
    JobQueue *queue = [[JobQueue alloc] initWithCPUs:1 dirs:1 files:1 defaults:NSUserDefaults.standardUserDefaults];
    [queue addJob:job];
    [queue wait];

    XCTAssertTrue(job.isDone);
    XCTAssertFalse(job.isFailed);
    XCTAssertTrue(job.isOptimized);
    NSData *after = [NSData dataWithContentsOfURL:path];
    XCTAssertLessThan(after.length, before.length);
    XCTAssertEqualObjects([[[File alloc] initWithData:after fromPath:path] mimeType], @"image/webp");
}


- (void)testAVIFBrands {
    NSURL *path = [NSURL fileURLWithPath:@"/tmp/image-without-extension"];
    unsigned char header[] = {0, 0, 0, 24, 'f', 't', 'y', 'p', 'm', 'i', 'f', '1',
                              0, 0, 0, 0, 'm', 'i', 'f', '1', 'a', 'v', 'i', 'f'};
    NSData *data = [NSData dataWithBytes:header length:sizeof(header)];
    XCTAssertEqualObjects([[[File alloc] initWithData:data fromPath:path] mimeType], @"image/avif");
    header[23] = 's';
    data = [NSData dataWithBytes:header length:sizeof(header)];
    XCTAssertEqualObjects([[[File alloc] initWithData:data fromPath:path] mimeType], @"image/avif");
    for (NSUInteger length = 0; length < sizeof(header); length++) {
        data = [NSData dataWithBytes:header length:length];
        XCTAssertNil([[[File alloc] initWithData:data fromPath:path] mimeType]);
    }
    header[23] = 'c'; // An unrelated HEIF brand is not AVIF.
    data = [NSData dataWithBytes:header length:sizeof(header)];
    XCTAssertNil([[[File alloc] initWithData:data fromPath:path] mimeType]);
}

- (void)testCompressAVIF {
    NSURL *original = [[NSBundle bundleForClass:self.class] URLForResource:@"unoptimized" withExtension:@"avif"];
    // No extension, matching the Finder extension's temporary input paths.
    NSURL *path = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString]];
    XCTAssertTrue([[NSFileManager defaultManager] copyItemAtURL:original toURL:path error:nil]);
    NSData *before = [NSData dataWithContentsOfURL:path];
    XCTAssertEqualObjects([[[File alloc] initWithData:before fromPath:path] mimeType], @"image/avif");
    Job *job = [[Job alloc] initWithFilePath:path resultsDatabase:nil];
    JobQueue *queue = [[JobQueue alloc] initWithCPUs:1 dirs:1 files:1 defaults:NSUserDefaults.standardUserDefaults];
    [queue addJob:job];
    [queue wait];
    XCTAssertTrue(job.isDone);
    XCTAssertFalse(job.isFailed);
    XCTAssertTrue(job.isOptimized);
    NSData *after = [NSData dataWithContentsOfURL:path];
    XCTAssertLessThan(after.length, before.length);
    XCTAssertEqualObjects([[[File alloc] initWithData:after fromPath:path] mimeType], @"image/avif");
    Job *second = [[Job alloc] initWithFilePath:path resultsDatabase:nil];
    [queue addJob:second];
    [queue wait];
    XCTAssertTrue(second.isDone);
    XCTAssertFalse(second.isFailed);
    XCTAssertEqualObjects(after, [NSData dataWithContentsOfURL:path]);
    XCTAssertTrue([[NSFileManager defaultManager] removeItemAtURL:path error:nil]);
}

- (void)testJXLSignatures {
    NSURL *path = [NSURL fileURLWithPath:@"/tmp/image-without-extension"];
    const unsigned char container[] = {0,0,0,12,'J','X','L',' ',13,10,135,10};
    const unsigned char codestream[] = {255,10,0,0,0,0};
    XCTAssertEqualObjects([[[File alloc] initWithData:[NSData dataWithBytes:container length:sizeof(container)] fromPath:path] mimeType], @"image/jxl");
    XCTAssertEqualObjects([[[File alloc] initWithData:[NSData dataWithBytes:codestream length:sizeof(codestream)] fromPath:path] mimeType], @"image/jxl");
    for (NSUInteger length = 0; length < sizeof(container); length++) {
        XCTAssertNil([[[File alloc] initWithData:[NSData dataWithBytes:container length:length] fromPath:path] mimeType]);
    }
}

- (void)testCompressJXL {
    NSURL *original = [[NSBundle bundleForClass:self.class] URLForResource:@"unoptimized" withExtension:@"jxl"];
    // No extension, matching the Finder extension's temporary input paths.
    NSURL *path = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString]];
    XCTAssertTrue([[NSFileManager defaultManager] copyItemAtURL:original toURL:path error:nil]);
    NSData *before = [NSData dataWithContentsOfURL:path];
    XCTAssertEqualObjects([[[File alloc] initWithData:before fromPath:path] mimeType], @"image/jxl");
    Job *job = [[Job alloc] initWithFilePath:path resultsDatabase:nil];
    JobQueue *queue = [[JobQueue alloc] initWithCPUs:1 dirs:1 files:1 defaults:NSUserDefaults.standardUserDefaults];
    [queue addJob:job];
    [queue wait];
    XCTAssertTrue(job.isDone);
    XCTAssertFalse(job.isFailed);
    XCTAssertTrue(job.isOptimized);
    NSData *after = [NSData dataWithContentsOfURL:path];
    XCTAssertLessThan(after.length, before.length);
    XCTAssertEqualObjects([[[File alloc] initWithData:after fromPath:path] mimeType], @"image/jxl");
    Job *second = [[Job alloc] initWithFilePath:path resultsDatabase:nil];
    [queue addJob:second];
    [queue wait];
    XCTAssertTrue(second.isDone);
    XCTAssertFalse(second.isFailed);
    XCTAssertEqualObjects(after, [NSData dataWithContentsOfURL:path]);
    XCTAssertTrue([[NSFileManager defaultManager] removeItemAtURL:path error:nil]);
}

@end
