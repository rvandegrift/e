#ifndef MIXER_H
#define MIXER_H

#include "e.h"
#include <Eina.h>
#include "emix.h"
#include "../e_mod_config.h"

EINTERN void *e_modapi_gadget_init(E_Module *m);
EINTERN int   e_modapi_gadget_shutdown(E_Module *m);
EINTERN int   e_modapi_gadget_save(E_Module *m);

EINTERN extern int _e_gemix_log_domain;

#undef DBG
#undef INF
#undef WRN
#undef ERR
#undef CRIT
#define DBG(...) EINA_LOG_DOM_DBG(_e_gemix_log_domain, __VA_ARGS__)
#define INF(...) EINA_LOG_DOM_INF(_e_gemix_log_domain, __VA_ARGS__)
#define WRN(...) EINA_LOG_DOM_WARN(_e_gemix_log_domain, __VA_ARGS__)
#define ERR(...) EINA_LOG_DOM_ERR(_e_gemix_log_domain, __VA_ARGS__)
#define CRIT(...) EINA_LOG_DOM_CRIT(_e_gemix_log_domain, __VA_ARGS__)

EINTERN Eina_Bool mixer_init(void);
EINTERN void mixer_shutdown(void);
EINTERN Evas_Object *mixer_gadget_create(Evas_Object *parent, int *id EINA_UNUSED, E_Gadget_Site_Orient orient EINA_UNUSED);

#endif
