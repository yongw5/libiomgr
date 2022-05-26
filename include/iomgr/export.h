#ifndef LIBIOMGR_INCLUDE_EXPORT_H_
#define LIBIOMGR_INCLUDE_EXPORT_H_

#if !defined(IOMGR_EXPORT)

#if defined(LIBIOMGR_SHARED_LIBRARY)

#if defined(IOMGR_COMPILE_LIBRARY)
#define IOMGR_EXPORT __attribute__((visibility("default")))
#else
#define IOMGR_EXPORT
#endif

#else  // defined(LIBIOMGR_SHARED_LIBRARY)
#define IOMGR_EXPORT
#endif

#endif  // !defined(IOMGR_EXPORT)

#endif  // LIBIOMGR_INCLUDE_EXPORT_H_