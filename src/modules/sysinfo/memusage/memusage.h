#ifndef MEMUSAGE_H
#define MEMUSAGE_H

#include "../sysinfo.h"

EINTERN void _memusage_config_updated(Instance *inst);
EINTERN Evas_Object *memusage_configure(Instance *inst);

EINTERN void _memusage_proc_getusage(unsigned long *mem_total,
                             unsigned long *mem_used,
                             unsigned long *mem_cached,
                             unsigned long *mem_buffers,
                             unsigned long *mem_shared,
                             unsigned long *swp_total,
                             unsigned long *swp_used);

EINTERN void _memusage_sysctl_getusage(unsigned long *mem_total,
                             unsigned long *mem_used,
                             unsigned long *mem_cached,
                             unsigned long *mem_buffers,
                             unsigned long *mem_shared,
                             unsigned long *swp_total,
                             unsigned long *swp_used);

#endif
