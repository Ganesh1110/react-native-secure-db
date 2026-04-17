#import <Foundation/Foundation.h>
#import <React/RCTBridgeModule.h>
#import "TurboDBImpl.h"

@interface TurboDBInstaller : NSObject <RCTBridgeModule>
@end

@implementation TurboDBInstaller

RCT_EXPORT_MODULE(TurboDBInstaller);


RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(install)
{
  // This creates a stub - actual JSI installation will be done via TurboModule
  return @true;
}

RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(getDocumentDirectory)
{
  NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
  return [paths firstObject];
}

@end
