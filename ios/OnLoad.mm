#import <Foundation/Foundation.h>
#import "TurboDBImpl.h"
#import <ReactCommon/CxxTurboModuleUtils.h>

@interface TurboDBOnLoad : NSObject
@end

@implementation TurboDBOnLoad

using namespace facebook::react;

+ (void)load
{
  registerCxxModuleToGlobalModuleMap(
    std::string(NativeTurboDBCxxSpec<TurboDBImpl>::kModuleName),
    [](std::shared_ptr<CallInvoker> jsInvoker) {
      return std::make_shared<TurboDBImpl>(jsInvoker);
    }
  );
}

@end
