#include "TurboDBImpl.h"
#include "DBEngine.h"
#include "SodiumCryptoContext.h"

#if __has_include(<TurboDBSpecJSI.h>)
#include <TurboDBSpecJSI.h>
#elif __has_include("TurboDBSpecJSI.h")
#include "TurboDBSpecJSI.h"
#endif

#ifdef __APPLE__
#import <Foundation/Foundation.h>
#include "KeyManagerIOS.h"
#include "PlatformUtilsIOS.h"
#endif

namespace facebook::react {

TurboDBImpl::TurboDBImpl(std::shared_ptr<CallInvoker> jsInvoker)
    : NativeTurboDBCxxSpec<TurboDBImpl>(std::move(jsInvoker)) {}

void TurboDBImpl::install(jsi::Runtime& rt) {
    auto js_invoker = this->jsInvoker_;
    
#ifdef __APPLE__
    std::string docsDir = getDocumentsDirectory(rt);
    
    auto masterKey = turbo_db::KeyManagerIOS::getOrGenerateMasterKey("TurboDBMasterKey");
    
    auto crypto = std::make_unique<turbo_db::SodiumCryptoContext>();
    crypto->setMasterKey(masterKey);
    turbo_db::installDBEngine(rt, js_invoker, std::move(crypto));
#endif
}

std::string TurboDBImpl::getDocumentsDirectory(jsi::Runtime& rt) {
#ifdef __APPLE__
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *basePath = [paths firstObject];
    if (basePath == nil) {
        return "";
    }
    return [basePath UTF8String];
#else
    return "";
#endif
}

std::string TurboDBImpl::getVersion(jsi::Runtime& rt) {
    return "0.1.0";
}

}
