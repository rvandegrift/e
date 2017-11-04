#include "luncher.h"

#define CONFIG_VERSION 2

static E_Config_DD *conf_edd = NULL;
static E_Config_DD *conf_item_edd = NULL;
Eina_List *luncher_instances = NULL;
Config *luncher_config = NULL;

EINTERN void
luncher_init(void)
{
   Eina_List *l;
   Config_Item *ci;

   conf_item_edd = E_CONFIG_DD_NEW("Luncher_Config_Item", Config_Item);
#undef T
#undef D
#define T Config_Item
#define D conf_item_edd
   E_CONFIG_VAL(D, T, id, INT);
   E_CONFIG_VAL(D, T, version, INT);
   E_CONFIG_VAL(D, T, preview_size, INT);
   E_CONFIG_VAL(D, T, dir, STR);
   E_CONFIG_VAL(D, T, type, INT);

   conf_edd = E_CONFIG_DD_NEW("Luncher_Config", Config);
#undef T
#undef D
#define T Config
#define D conf_edd
   E_CONFIG_LIST(D, T, items, conf_item_edd);

   luncher_config = e_config_domain_load("module.luncher", conf_edd);

   if (!luncher_config)
     {
        luncher_config = E_NEW(Config, 1);
        ci = E_NEW(Config_Item, 1);
        ci->id = 0;
        ci->version = CONFIG_VERSION;
        ci->preview_size = 64;
        ci->dir = eina_stringshare_add("default");
        ci->type = E_LUNCHER_MODULE_FULL;
        luncher_config->items = eina_list_append(luncher_config->items, ci);
     }
   EINA_LIST_FOREACH(luncher_config->items, l, ci)
     {
        if (ci->version < CONFIG_VERSION)
          {
             ci->version = CONFIG_VERSION;
             ci->type = E_LUNCHER_MODULE_FULL;
          }
     }
   e_gadget_type_add("Luncher Bar", bar_create, NULL);
   e_gadget_type_add("Luncher Grid", grid_create, NULL);
}

EINTERN void
luncher_shutdown(void)
{
   if (luncher_config)
     {
        Config_Item *ci;
        EINA_LIST_FREE(luncher_config->items, ci)
          {
             eina_stringshare_del(ci->dir);
             free(ci);
          }
        E_FREE(luncher_config);
     }
   E_CONFIG_DD_FREE(conf_edd);
   E_CONFIG_DD_FREE(conf_item_edd);

   e_gadget_type_del("Luncher Bar");
   e_gadget_type_del("Luncher Grid");
}

E_API E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION,
   "Luncher"
};

E_API void *
e_modapi_init(E_Module *m)
{
   luncher_init();

   luncher_config->module = m;
   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   luncher_shutdown();
   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.luncher", conf_edd, luncher_config);
   return 1;
}
