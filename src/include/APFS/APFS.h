#ifndef _APFS_H_
#define _APFS_H_

#include <os/base.h>
#include <os/lock.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <APFS/APFSConstants.h>

__BEGIN_DECLS

kern_return_t
APFSCancelContainerResize(os_unfair_lock_t lock);

kern_return_t
APFSContainerDefrag(const char *bsdName);

kern_return_t
APFSContainerEFIEmbed(const char *bsdName, const char **Ptr, size_t PtrSize);

kern_return_t
APFSContainerEFIGetVersion(const char *bsdName, const char **Ptr, size_t PtrSize, void *outputStruct);

kern_return_t
APFSContainerGetBootDevice(CFStringRef *container);

kern_return_t
APFSContainerGetDefrag(const char *bsdName, int *buf);

kern_return_t
APFSContainerGetFreeExtentHistogram(io_service_t device, CFDictionaryRef dict);

kern_return_t
APFSContainerGetMaxVolumeCount(const char *device, CFIndex *buf);

kern_return_t
APFSContainerGetMinimalSize(const char *device, CFIndex *buf);

kern_return_t
APFSContainerMigrateMediaKeys(const char *container);

kern_return_t
APFSContainerSetDefrag(const char *bsdName, int defrag);

kern_return_t
APFSContainerStitchVolumeGroup(const char *bsdName);

kern_return_t
APFSContainerVolumeGroupRemove(const char *bsdName, uuid_t uuid);

kern_return_t
APFSContainerVolumeGroupSyncUnlockRecords(const char *bsdName, uuid_t uuid);

kern_return_t
APFSContainerWipeVolumeKeys(const char *bsdName);

kern_return_t
APFSExtendedSpaceInfo(const char *device, CFDictionaryRef dict);

kern_return_t
APFSGetVolumeGroupID(const char *device, uuid_t uuid);

kern_return_t
APFSVolumeCreate(const char *device, CFDictionaryRef dict);

kern_return_t
APFSVolumeCreateForMSU(const char *device, CFDictionaryRef dict);

kern_return_t
APFSVolumeDelete(const char *device);

kern_return_t
APFSVolumeRole(const char *device, short *role, CFMutableArrayRef *buf);

kern_return_t
APFSVolumeRoleFind(const char *device, short role, CFMutableArrayRef *buf);

__END_DECLS

#endif
