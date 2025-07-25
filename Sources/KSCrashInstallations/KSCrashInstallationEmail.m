//
//  KSCrashInstallationEmail.m
//
//  Created by Karl Stenerud on 2013-03-02.
//
//  Copyright (c) 2012 Karl Stenerud. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall remain in place
// in this source code.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#import "KSCrashInstallationEmail.h"
#import "KSCrashInstallation+Private.h"
#import "KSCrashReportFilterAlert.h"
#import "KSCrashReportSinkEMail.h"
#import "KSNSErrorHelper.h"

@interface KSCrashInstallationEmail ()

@property(nonatomic, readwrite, copy) NSDictionary *defaultFilenameFormats;

@end

@implementation KSCrashInstallationEmail

+ (instancetype)sharedInstance
{
    static KSCrashInstallationEmail *sharedInstance = nil;
    static dispatch_once_t onceToken;

    dispatch_once(&onceToken, ^{
        sharedInstance = [[KSCrashInstallationEmail alloc] init];
    });
    return sharedInstance;
}

- (instancetype)init
{
    if ((self = [super init])) {
        NSString *bundleName = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleName"];
        _subject = [NSString stringWithFormat:@"Crash Report (%@)", bundleName];
        _defaultFilenameFormats = [NSDictionary
            dictionaryWithObjectsAndKeys:[NSString stringWithFormat:@"crash-report-%@-%%d.txt.gz", bundleName],
                                         [NSNumber numberWithInt:KSCrashEmailReportStyleApple],
                                         [NSString stringWithFormat:@"crash-report-%@-%%d.json.gz", bundleName],
                                         [NSNumber numberWithInt:KSCrashEmailReportStyleJSON], nil];
        [self setReportStyle:KSCrashEmailReportStyleJSON useDefaultFilenameFormat:YES];
    }
    return self;
}

- (BOOL)validateSetupWithError:(NSError **)error
{
    if ([super validateSetupWithError:error] == NO) {
        return NO;
    }

    if (self.recipients.count == 0) {
        if (error != NULL) {
            *error = [KSNSErrorHelper errorWithDomain:[[self class] description]
                                                 code:0
                                          description:@"Empty recepients array"];
        }
        return NO;
    }

    if (self.subject.length == 0) {
        if (error != NULL) {
            *error = [KSNSErrorHelper errorWithDomain:[[self class] description]
                                                 code:0
                                          description:@"No email subject provided"];
        }
        return NO;
    }

    if (self.filenameFmt.length == 0) {
        if (error != NULL) {
            *error = [KSNSErrorHelper errorWithDomain:[[self class] description]
                                                 code:0
                                          description:@"No filename format provided"];
        }
        return NO;
    }

    return YES;
}

- (void)setReportStyle:(KSCrashEmailReportStyle)reportStyle useDefaultFilenameFormat:(BOOL)useDefaultFilenameFormat
{
    self.reportStyle = reportStyle;

    if (useDefaultFilenameFormat) {
        self.filenameFmt = [self.defaultFilenameFormats objectForKey:[NSNumber numberWithInt:(int)reportStyle]];
    }
}

- (id<KSCrashReportFilter>)sink
{
    KSCrashReportSinkEMail *sink = [[KSCrashReportSinkEMail alloc] initWithRecipients:self.recipients
                                                                              subject:self.subject
                                                                              message:self.message
                                                                          filenameFmt:self.filenameFmt];

    switch (self.reportStyle) {
        case KSCrashEmailReportStyleApple:
            return [sink defaultCrashReportFilterSetAppleFmt];
        case KSCrashEmailReportStyleJSON:
            return [sink defaultCrashReportFilterSet];
        default:
            return nil;
    }
}

@end
