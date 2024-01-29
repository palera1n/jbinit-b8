SHELL := /usr/bin/env bash
SRC = $(shell pwd)/src
CC = xcrun -sdk iphoneos clang
STRIP = xcrun -sdk iphoneos strip
SED = gsed
I_N_T = install_name_tool
CFLAGS += -I$(SRC) -isystem $(SRC)/../apple-include -I$(SRC)/include -flto=full
ifeq ($(ASAN),1)
CFLAGS += -DASAN
endif
ifeq ($(DEV_BUILD),1)
CFLAGS += -DDEV_BUILD
DEV_TARGETS += xpchook.dylib
endif

TARGET_SYSROOT != xcrun -sdk iphoneos --show-sdk-path
MACOSX_SYSROOT != xcrun -sdk macosx --show-sdk-path

ifeq ($(ASAN),1)
RAMDISK_SIZE = 8M
else ifeq ($(DEV_BUILD),1)
RAMDISK_SIZE = 1M
else
RAMDISK_SIZE = 512K
endif

export SRC CC CFLAGS LDFLAGS STRIP I_N_T

all: ramdisk.dmg

binpack.dmg: apple-include binpack.tar loader.dmg hook_all
	sudo rm -rf ./binpack.dmg binpack
	sudo mkdir binpack
	sudo tar -C binpack --preserve-permissions -xf binpack.tar
	sudo rm -rf binpack/usr/share cores
	sudo ln -sf /cores/jbloader binpack/usr/sbin/p1ctl
	sudo mkdir -p binpack/Applications
	sudo mkdir -p binpack/usr/lib
	sudo mkdir -p binpack/Library/Frameworks/CydiaSubstrate.framework
	sudo mkdir -p binpack/Library/LaunchDaemons
	sudo cp -a dropbear-plist/*.plist binpack/Library/LaunchDaemons
	sudo cp src/systemhooks/rootlesshooks.dylib binpack/usr/lib
	sudo cp loader.dmg binpack
	sudo cp src/systemhooks/libellekit.dylib binpack/usr/lib
	sudo ln -s ../../../usr/lib/libellekit.dylib binpack/Library/Frameworks/CydiaSubstrate.framework/CydiaSubstrate
	sudo chown -R 0:0 binpack
	hdiutil create -size 8m -layout NONE -format UDZO -imagekey zlib-level=9 -srcfolder ./binpack -volname palera1nfs -fs HFS+ ./binpack.dmg
	sudo rm -rf binpack

ramdisk.dmg: apple-include jbinit jbloader payload.dylib $(DEV_TARGETS)
	$(MAKE) -C $(SRC)
	rm -f ramdisk.dmg
	sudo rm -rf ramdisk
	mkdir -p ramdisk
	mkdir -p ramdisk/{usr/lib,sbin,jbin,dev,mnt}
	ln -s /jbin/jbloader ramdisk/sbin/launchd
	ln -s /sbin/launchd ramdisk/jbin/launchd
	mkdir -p ramdisk/usr/lib
	cp $(SRC)/jbinit/jbinit ramdisk/usr/lib/dyld
	cp $(SRC)/systemhooks/payload.dylib $(SRC)/jbloader/jbloader ramdisk/jbin
ifeq ($(DEV_BUILD),1)
	cp $(SRC)/jbloader/launchctl/tools/xpchook.dylib ramdisk/jbin
endif
ifeq ($(ASAN),1)
	cp $(shell xcode-select -p)/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/*//lib/darwin/libclang_rt.{asan,ubsan}_ios_dynamic.dylib ramdisk/jbin
endif
	sudo gchown -R 0:0 ramdisk
	hdiutil create -size $(RAMDISK_SIZE) -layout NONE -format UDRW -uid 0 -gid 0 -srcfolder ./ramdisk -fs HFS+ -volname palera1nrd ./ramdisk.dmg

loader.dmg: palera1nLoader.ipa
	rm -rf loader.dmg Payload
	unzip palera1nLoader.ipa
	hdiutil create -size 2m -layout NONE -format ULFO -uid 0 -gid 0 -volname palera1nLoader -srcfolder ./Payload -fs HFS+ ./loader.dmg
	rm -rf Payload

apple-include:
	mkdir -p apple-include/{bsm,objc,os/internal,sys,firehose,CoreFoundation,FSEvents,IOSurface,IOKit/kext,libkern,kern,arm,{mach/,}machine,CommonCrypto,Security,CoreSymbolication,Kernel/{kern,IOKit,libkern},rpc,rpcsvc,xpc/private,ktrace,mach-o,dispatch}
	cp -af $(MACOSX_SYSROOT)/usr/include/{arpa,bsm,hfs,net,xpc,netinet,servers,timeconv.h,launch.h} apple-include
	cp -af $(MACOSX_SYSROOT)/usr/include/objc/objc-runtime.h apple-include/objc
	cp -af $(MACOSX_SYSROOT)/usr/include/libkern/{OSDebug.h,OSKextLib.h,OSReturn.h,OSThermalNotification.h,OSTypes.h,machine} apple-include/libkern
	cp -af $(MACOSX_SYSROOT)/usr/include/kern apple-include
	cp -af $(MACOSX_SYSROOT)/usr/include/sys/{tty*,ptrace,kern*,random,reboot,user,vnode,disk,vmmeter,conf}.h apple-include/sys
	cp -af $(MACOSX_SYSROOT)/System/Library/Frameworks/Kernel.framework/Versions/Current/Headers/sys/disklabel.h apple-include/sys
	cp -af $(MACOSX_SYSROOT)/System/Library/Frameworks/IOKit.framework/Headers/{AppleConvergedIPCKeys.h,IOBSD.h,IOCFBundle.h,IOCFPlugIn.h,IOCFURLAccess.h,IOKitServer.h,IORPC.h,IOSharedLock.h,IOUserServer.h,audio,avc,firewire,graphics,hid,hidsystem,i2c,iokitmig.h,kext,ndrvsupport,network,ps,pwr_mgt,sbp2,scsi,serial,storage,stream,usb,video} apple-include/IOKit
	cp -af $(MACOSX_SYSROOT)/System/Library/Frameworks/Security.framework/Headers/{mds_schema,oidsalg,SecKeychainSearch,certextensions,Authorization,eisl,SecDigestTransform,SecKeychainItem,oidscrl,cssmcspi,CSCommon,cssmaci,SecCode,CMSDecoder,oidscert,SecRequirement,AuthSession,SecReadTransform,oids,cssmconfig,cssmkrapi,SecPolicySearch,SecAccess,cssmtpi,SecACL,SecEncryptTransform,cssmapi,cssmcli,mds,x509defs,oidsbase,SecSignVerifyTransform,cssmspi,cssmkrspi,SecTask,cssmdli,SecAsn1Coder,cssm,SecTrustedApplication,SecCodeHost,SecCustomTransform,oidsattr,SecIdentitySearch,cssmtype,SecAsn1Types,emmtype,SecTransform,SecTrustSettings,SecStaticCode,emmspi,SecTransformReadTransform,SecKeychain,SecDecodeTransform,CodeSigning,AuthorizationPlugin,cssmerr,AuthorizationTags,CMSEncoder,SecEncodeTransform,SecureDownload,SecAsn1Templates,AuthorizationDB,SecCertificateOIDs,cssmapple}.h apple-include/Security
	cp -af $(MACOSX_SYSROOT)/usr/include/{ar,bootstrap,launch,libc,libcharset,localcharset,nlist,NSSystemDirectories,tzfile,vproc}.h apple-include
	cp -af $(MACOSX_SYSROOT)/usr/include/mach/{*.defs,{mach_vm,shared_region}.h} apple-include/mach
	cp -af $(MACOSX_SYSROOT)/usr/include/mach/machine/*.defs apple-include/mach/machine
	cp -af $(MACOSX_SYSROOT)/usr/include/rpc/pmap_clnt.h apple-include/rpc
	cp -af $(MACOSX_SYSROOT)/usr/include/rpcsvc/yp{_prot,clnt}.h apple-include/rpcsvc
	cp -af $(TARGET_SYSROOT)/usr/include/mach/machine/thread_state.h apple-include/mach/machine
	cp -af $(TARGET_SYSROOT)/usr/include/mach/arm apple-include/mach
	cp -af $(MACOSX_SYSROOT)/System/Library/Frameworks/IOKit.framework/Headers/* apple-include/IOKit
	cp -af $(MACOSX_SYSROOT)/System/Library/Frameworks/IOSurface.framework/Headers/* apple-include/IOSurface
	$(SED) -E s/'__IOS_PROHIBITED|__TVOS_PROHIBITED|__WATCHOS_PROHIBITED'//g < $(TARGET_SYSROOT)/usr/include/stdlib.h > apple-include/stdlib.h
	$(SED) -E s/'__IOS_PROHIBITED|__TVOS_PROHIBITED|__WATCHOS_PROHIBITED'//g < $(TARGET_SYSROOT)/usr/include/time.h > apple-include/time.h
	$(SED) -E s/'__IOS_PROHIBITED|__TVOS_PROHIBITED|__WATCHOS_PROHIBITED'//g < $(TARGET_SYSROOT)/usr/include/unistd.h > apple-include/unistd.h
	$(SED) -E s/'__IOS_PROHIBITED|__TVOS_PROHIBITED|__WATCHOS_PROHIBITED'//g < $(TARGET_SYSROOT)/usr/include/mach/task.h > apple-include/mach/task.h
	$(SED) -E s/'__IOS_PROHIBITED|__TVOS_PROHIBITED|__WATCHOS_PROHIBITED'//g < $(TARGET_SYSROOT)/usr/include/mach/mach_host.h > apple-include/mach/mach_host.h
	$(SED) -E s/'__IOS_PROHIBITED|__TVOS_PROHIBITED|__WATCHOS_PROHIBITED'//g < $(TARGET_SYSROOT)/usr/include/ucontext.h > apple-include/ucontext.h
	$(SED) -E s/'__IOS_PROHIBITED|__TVOS_PROHIBITED|__WATCHOS_PROHIBITED'//g < $(TARGET_SYSROOT)/usr/include/signal.h > apple-include/signal.h
	$(SED) 's/#ifndef __OPEN_SOURCE__/#if 1\n#if defined(__has_feature) \&\& defined(__has_attribute)\n#if __has_attribute(availability)\n#define __API_AVAILABLE_PLATFORM_bridgeos(x) bridgeos,introduced=x\n#define __API_DEPRECATED_PLATFORM_bridgeos(x,y) bridgeos,introduced=x,deprecated=y\n#define __API_UNAVAILABLE_PLATFORM_bridgeos bridgeos,unavailable\n#endif\n#endif/g' < $(TARGET_SYSROOT)/usr/include/AvailabilityInternal.h > apple-include/AvailabilityInternal.h
	$(SED) -E /'__API_UNAVAILABLE'/d < $(TARGET_SYSROOT)/usr/include/pthread.h > apple-include/pthread.h
	@if [ -f $(TARGET_SYSROOT)/System/Library/Frameworks/CoreFoundation.framework/Headers/CFUserNotification.h ]; then $(SED) -E 's/API_UNAVAILABLE\(ios, watchos, tvos\)//g' < $(TARGET_SYSROOT)/System/Library/Frameworks/CoreFoundation.framework/Headers/CFUserNotification.h > apple-include/CoreFoundation/CFUserNotification.h; fi
	$(SED) -i -E s/'__API_UNAVAILABLE\(.*\)'// apple-include/IOKit/IOKitLib.h
	$(SED) -i -E s/'API_UNAVAILABLE(.*)'// apple-include/xpc/connection.h

$(SRC)/dyld_platform_test/dyld_platform_test:
	$(MAKE) -C $(SRC)/dyld_platform_test

dyld_platform_test: $(SRC)/dyld_platform_test/dyld_platform_test

xpchook.dylib:
	$(MAKE) -C src/jbloader xpchook.dylib

clean:
	rm -f payload.dylib binpack.dmg src/launchctl/tools/xpchook.dylib src/systemhooks/libellekit.dylib ramdisk.dmg \
		src/jbinit/jbinit src/jbloader/jbloader src/systemhooks/payload.dylib \
		src/jbloader/loader/create_fakefs_sh.c src/dyld_platform_test/dyld_platform_test loader.dmg \
		src/systemhooks/rootlesshooks.dylib
	sudo rm -rf ramdisk binpack cores
	rm -rf src/systemhooks/ellekit/build src/systemhooks/rootlesshooks/.theos
	find . -name '*.o' -delete
	rm -f ramdisk.img4

hook_all:
	$(MAKE) -C src/systemhooks all

.PHONY: all clean jbinit jbloader payload.dylib dyld_platform_test xpchook.dylib binpack.dmg hook_all apple-include
