#ifndef BATMAN_H
#define BATMAN_H

#include "../sysinfo.h"

#define CHECK_NONE      0
#define CHECK_ACPI      1
#define CHECK_APM       2
#define CHECK_PMU       3
#define CHECK_SYS_ACPI  4

#define UNKNOWN 0
#define NOSUBSYSTEM 1
#define SUBSYSTEM 2

#define SUSPEND 0
#define HIBERNATE 1
#define SHUTDOWN 2

#define POPUP_DEBOUNCE_CYCLES  2

typedef struct _Battery Battery;
typedef struct _Ac_Adapter Ac_Adapter;
typedef struct _Batman_Config Batman_Config;

struct _Battery
{
   Instance *inst;
   const char *udi;
#if defined(HAVE_EEZE) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__)
   Ecore_Poller *poll;
#endif
   Eina_Bool present:1;
   Eina_Bool charging:1;
#if defined(HAVE_EEZE) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__)
   double last_update;
   double percent;
   double current_charge;
   double design_charge;
   double last_full_charge;
   double charge_rate;
   double time_full;
   double time_left;
#else
   int percent;
   int current_charge;
   int design_charge;
   int last_full_charge;
   int charge_rate;
   int time_full;
   int time_left;
   const char *type;
   const char *charge_units;
#endif
   const char *technology;
   const char *model;
   const char *vendor;
   Eina_Bool got_prop:1;
   Eldbus_Proxy *proxy;
   int * mib;
#if defined(__FreeBSD__) || defined(__DragonFly__)
   int * mib_state;
   int * mib_units;
   int * mib_time;
   int batteries;
   int time_min;
#endif
};

struct _Ac_Adapter
{
   Instance *inst;
   const char *udi;
   Eina_Bool present:1;
   const char *product;
   Eldbus_Proxy *proxy;
   int * mib;
};

struct _Batman_Config
{
   Instance *inst;
   Evas_Object *alert_check;
   Evas_Object *alert_desktop;
   Evas_Object *alert_time;
   Evas_Object *alert_percent;
   Evas_Object *alert_timeout;
   Evas_Object *general_page;
   Evas_Object *alert_page;
   Evas_Object *power_page;
};

EINTERN Eina_List *_batman_battery_find(const char *udi);
EINTERN Eina_List *_batman_ac_adapter_find(const char *udi);
EINTERN void _batman_update(Instance *inst, int full, int time_left, Eina_Bool have_battery, Eina_Bool have_power);
EINTERN void _batman_device_update(Instance *inst);
/* in batman_fallback.c */
EINTERN int _batman_fallback_start(Instance *inst);
EINTERN void _batman_fallback_stop(void);
/* end batman_fallback.c */
#if defined(HAVE_EEZE)
/* in batman_udev.c */
EINTERN int  _batman_udev_start(Instance *inst);
EINTERN void _batman_udev_stop(Instance *inst);
/* end batman_udev.c */
#elif !defined(__OpenBSD__) && !defined(__DragonFly__) && !defined(__FreeBSD__) && !defined(__NetBSD__)
/* in batman_upower.c */
EINTERN int _batman_upower_start(Instance *inst);
EINTERNvoid _batman_upower_stop(void);
/* end batman_upower.c */
#else
/* in batman_sysctl.c */
EINTERN int _batman_sysctl_start(Instance *inst);
EINTERN void _batman_sysctl_stop(void);
/* end batman_sysctl.c */
#endif

EINTERN Evas_Object *batman_configure(Instance *inst);
EINTERN void _batman_config_updated(Instance *inst);

#endif
