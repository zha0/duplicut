#include <stdlib.h>
#include <stdio.h>
#include "meminfo.h"
#include "error.h"
#include "bytesize.h"

#define PROC_MEMINFO_FILE   ("/proc/meminfo")
#define BUF_SIZE            (1024)


# ifdef __APPLE__ /* Mac OS X case (host_statistics64()) */

#  include <mach/vm_statistics.h>
#  include <mach/mach_types.h>
#  include <mach/mach_init.h>
#  include <mach/mach_host.h>

/** Apple Mac OS X specific wrapper for meminfo_memavailable()
 *
 * Specific code needed because Mac OS X has no /proc vfs.
 */
static long meminfo_memavailable(void)
{
    vm_size_t               page_size;
    mach_port_t             mach_port;
    mach_msg_type_number_t  count;
    vm_statistics64_data_t  vm_stats;

    mach_port = mach_host_self();
    count = sizeof(vm_stats) / sizeof(natural_t);
    if (host_page_size(mach_port, &page_size) != KERN_SUCCESS)
    {
        error("Couldn't get page size from host_page_size()");
    }
    if (host_statistics64(mach_port, HOST_VM_INFO,
                (host_info64_t)&vm_stats, &count) != KERN_SUCCESS)
    {
        error("Couldn't get vm iunfo from host_statistics64()");
    }
    return ((int64_t)vm_stats.free_count * (int64_t)page_size);
}

# else /* Unix default case (/proc/meminfo) */

/** Retrieve value from given /proc/meminfo line.
 */
static long get_value(char *ptr, const char *str, size_t str_len)
{
    if (strncmp(ptr, str, str_len) == 0)
    {
        ptr += str_len;
        if (*ptr == ':')
        {
            ptr += 1;
            return (bytesize(ptr));
        }
    }
    return (-1L);
}


/** Get identifier value from /proc/meminfo
 *
 * The proc_meminfo() function returns the value in bytes of
 * an identifier from /proc/meminfo file.
 *
 * If identifier could not be found, the function returns -1.
 */
static long proc_meminfo(const char *identifier)
{
    char    *buf;
    size_t  size;
    FILE    *fp;
    long    result;
    size_t  identifier_len;

    fp = fopen(PROC_MEMINFO_FILE, "r");
    if (fp == NULL)
    {
        error("cannot open %s: %s", PROC_MEMINFO_FILE, ERRNO);
    }
    size = BUF_SIZE * sizeof(*buf);
    buf = (char*) malloc(size);
    if (buf == NULL)
    {
        die("malloc()");
    }

    result = -1L;
    identifier_len = strlen(identifier);
    while (getline(&buf, &size, fp) >= 0)
    {
        result = get_value(buf, identifier, identifier_len);
        if (result >= 0)
        {
            break;
        }
    }
    fclose(fp);
    free((void*) buf);
    return (result);
}


/** Default meminfo_memavailable() function.
 *
 * This wrapper calls /proc/meminfo through proc_meminfo()
 * static function.
 *
 * "MemAvailable" represents memory which can really be
 * used without swapping, taking care of important things
 * such as shared memory segments, tmpfs, ramfs and
 * reclaimable slab memory.
 *
 * Therefore, it is only available on recent linux kernels,
 * so we fallback to "MemFree" if "MemAvailable" is not
 * provided.
 */
static long meminfo_memavailable(void)
{
    long result;

    result = meminfo("MemAvailable");
    if (result < 0)
    {
        result = meminfo("MemFree");
    }
    return (result);
}
# endif


/** Return requested meminfo parameter.
 *
 * If it fails to retrieve information, the function
 * returns -1.
 */
/* long        meminfo(enum e_meminfo_param info) */
long    meminfo(int info)
{
    switch (info)
    {
        case MEMAVAILABLE:
            return (meminfo_memavailable());
        default:
            error("meminfo(): Invalid argument: %d", info);
    }
    return (-1L);
}