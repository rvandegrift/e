#include "memusage.h"

typedef struct _Thread_Config Thread_Config;

struct _Thread_Config
{
   int                  interval;
   Instance            *inst;
   int                  mem_percent;
   int                  swp_percent;
   unsigned long        mem_total;
   unsigned long        mem_used;
   unsigned long        mem_cached;
   unsigned long        mem_buffers;
   unsigned long        mem_shared;
   unsigned long        swp_total;
   unsigned long        swp_used;
   E_Powersave_Sleeper *sleeper;
};

static void
_memusage_popup_update(Instance *inst)
{
   Evas_Object *pbar;
   int val_mb, val_perc;
   char buf[128];

   if (!inst->cfg->memusage.popup)
     return;

   if (inst->cfg->memusage.mem_total)
     {
        pbar = evas_object_data_get(inst->cfg->memusage.popup, "mem_used_pbar");
        val_mb = inst->cfg->memusage.mem_used / 1024;
        val_perc = 100 * ((float)inst->cfg->memusage.mem_used /
                          (float)inst->cfg->memusage.mem_total);
        snprintf(buf, sizeof(buf), "%d MB (%d %%)", val_mb, val_perc);
        elm_progressbar_value_set(pbar, (float)val_perc / 100);
        elm_progressbar_unit_format_set(pbar, buf);

        pbar = evas_object_data_get(inst->cfg->memusage.popup, "mem_buffers_pbar");
        val_mb = inst->cfg->memusage.mem_buffers / 1024;
        val_perc = 100 * ((float)inst->cfg->memusage.mem_buffers /
                          (float)inst->cfg->memusage.mem_total);
        snprintf(buf, sizeof(buf), "%d MB (%d %%)", val_mb, val_perc);
        elm_progressbar_value_set(pbar, (float)val_perc / 100);
        elm_progressbar_unit_format_set(pbar, buf);

        pbar = evas_object_data_get(inst->cfg->memusage.popup, "mem_cached_pbar");
        val_mb = inst->cfg->memusage.mem_cached / 1024;
        val_perc = 100 * ((float)inst->cfg->memusage.mem_cached /
                          (float)inst->cfg->memusage.mem_total);
        snprintf(buf, sizeof(buf), "%d MB (%d %%)", val_mb, val_perc);
        elm_progressbar_value_set(pbar, (float)val_perc / 100);
        elm_progressbar_unit_format_set(pbar, buf);

        pbar = evas_object_data_get(inst->cfg->memusage.popup, "mem_shared_pbar");
        val_mb = inst->cfg->memusage.mem_shared / 1024;
        val_perc = 100 * ((float)inst->cfg->memusage.mem_shared /
                          (float)inst->cfg->memusage.mem_total);
        snprintf(buf, sizeof(buf), "%d MB (%d %%)", val_mb, val_perc);
        elm_progressbar_value_set(pbar, (float)val_perc / 100);
        elm_progressbar_unit_format_set(pbar, buf);
     }

   if (inst->cfg->memusage.swp_total)
     {
        pbar = evas_object_data_get(inst->cfg->memusage.popup, "swap_pbar");
        val_mb = inst->cfg->memusage.swp_used / 1024;
        val_perc = 100 * ((float)inst->cfg->memusage.swp_used /
                          (float)inst->cfg->memusage.swp_total);
        snprintf(buf, sizeof(buf), "%d MB (%d %%)", val_mb, val_perc);
        elm_progressbar_value_set(pbar, (float)val_perc / 100);
        elm_progressbar_unit_format_set(pbar, buf);
     }
}

static void
_memusage_face_update(Instance *inst)
{
   Edje_Message_Int_Set *msg;

   msg = malloc(sizeof(Edje_Message_Int_Set) + 9 * sizeof(int));
   EINA_SAFETY_ON_NULL_RETURN(msg);
   msg->count = 2;
   msg->val[0] = inst->cfg->memusage.mem_percent;
   msg->val[1] = inst->cfg->memusage.swp_percent;
   msg->val[2] = inst->cfg->memusage.mem_total;
   msg->val[3] = inst->cfg->memusage.mem_used;
   msg->val[4] = inst->cfg->memusage.mem_cached;
   msg->val[5] = inst->cfg->memusage.mem_buffers;
   msg->val[6] = inst->cfg->memusage.mem_shared;
   msg->val[7] = inst->cfg->memusage.swp_total;
   msg->val[8] = inst->cfg->memusage.swp_used;
   edje_object_message_send(elm_layout_edje_get(inst->cfg->memusage.o_gadget),
                            EDJE_MESSAGE_INT_SET, 1, msg);
   E_FREE(msg);

   if (inst->cfg->memusage.popup)
     _memusage_popup_update(inst);
}

static Evas_Object *
_memusage_configure_cb(Evas_Object *g)
{
   Instance *inst = evas_object_data_get(g, "Instance");

   if (!sysinfo_config) return NULL;
   if (inst->cfg->memusage.popup) return NULL;
   return memusage_configure(inst);
}

static void
_memusage_popup_dismissed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   E_FREE_FUNC(obj, evas_object_del);
   inst->cfg->memusage.popup = NULL;
}

static void
_memusage_popup_deleted(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   inst->cfg->memusage.popup = NULL;
}

static Evas_Object *
_memusage_popup_create(Instance *inst)
{
   Evas_Object *popup, *table, *label, *pbar;
   char buf[128], buf2[128];

   popup = elm_ctxpopup_add(e_comp->elm);
   elm_object_style_set(popup, "noblock");
   evas_object_smart_callback_add(popup, "dismissed",
                                  _memusage_popup_dismissed, inst);
   evas_object_event_callback_add(popup, EVAS_CALLBACK_DEL,
                                  _memusage_popup_deleted, inst);

   table = elm_table_add(popup);
   E_EXPAND(table);
   E_FILL(table);
   elm_object_content_set(popup, table);
   evas_object_show(table);

   snprintf(buf, sizeof(buf), _("Memory Usage (Available %ld MB)"),
            inst->cfg->memusage.mem_total / 1024);
   snprintf(buf2, sizeof(buf2), "<big><b>%s</b></big>", buf);

   label = elm_label_add(table);
   E_EXPAND(label); E_ALIGN(label, 0.5, 0.5);
   elm_object_text_set(label, buf2);
   elm_table_pack(table, label, 0, 0, 2, 1);
   evas_object_show(label);

   label = elm_label_add(table);
   E_ALIGN(label, 0.0, 0.5);
   elm_object_text_set(label, _("Used"));
   elm_table_pack(table, label, 0, 1, 1, 1);
   evas_object_show(label);

   pbar = elm_progressbar_add(table);
   E_EXPAND(pbar);
   E_FILL(pbar);
   elm_progressbar_span_size_set(pbar, 200 * e_scale);
   elm_table_pack(table, pbar, 1, 1, 1, 1);
   evas_object_show(pbar);
   evas_object_data_set(popup, "mem_used_pbar", pbar);

   label = elm_label_add(table);
   E_ALIGN(label, 0.0, 0.5);
   elm_object_text_set(label, _("Buffers"));
   elm_table_pack(table, label, 0, 2, 1, 1);
   evas_object_show(label);

   pbar = elm_progressbar_add(table);
   E_EXPAND(pbar);
   E_FILL(pbar);
   elm_progressbar_span_size_set(pbar, 200 * e_scale);
   elm_table_pack(table, pbar, 1, 2, 1, 1);
   evas_object_show(pbar);
   evas_object_data_set(popup, "mem_buffers_pbar", pbar);

   label = elm_label_add(table);
   E_ALIGN(label, 0.0, 0.5);
   elm_object_text_set(label, _("Cached"));
   elm_table_pack(table, label, 0, 3, 1, 1);
   evas_object_show(label);

   pbar = elm_progressbar_add(table);
   E_EXPAND(pbar);
   E_FILL(pbar);
   elm_progressbar_span_size_set(pbar, 200 * e_scale);
   elm_table_pack(table, pbar, 1, 3, 1, 1);
   evas_object_show(pbar);
   evas_object_data_set(popup, "mem_cached_pbar", pbar);

   label = elm_label_add(table);
   E_ALIGN(label, 0.0, 0.5);
   elm_object_text_set(label, _("Shared"));
   elm_table_pack(table, label, 0, 4, 1, 1);
   evas_object_show(label);

   pbar = elm_progressbar_add(table);
   E_EXPAND(pbar);
   E_FILL(pbar);
   elm_progressbar_span_size_set(pbar, 200 * e_scale);
   elm_table_pack(table, pbar, 1, 4, 1, 1);
   evas_object_show(pbar);
   evas_object_data_set(popup, "mem_shared_pbar", pbar);

   snprintf(buf, sizeof(buf), _("Swap Usage (Available %ld MB)"),
            inst->cfg->memusage.swp_total / 1024);
   snprintf(buf2, sizeof(buf2), "<big><b>%s</b></big>", buf);

   label = elm_label_add(table);
   E_EXPAND(label);
   E_ALIGN(label, 0.5, 0.5);
   elm_object_text_set(label, buf2);
   elm_table_pack(table, label, 0, 5, 2, 1);
   evas_object_show(label);

   pbar = elm_progressbar_add(table);
   E_EXPAND(pbar);
   E_FILL(pbar);
   elm_table_pack(table, pbar, 0, 6, 2, 1);
   evas_object_show(pbar);
   evas_object_data_set(popup, "swap_pbar", pbar);

   // place and show the popup
   e_gadget_util_ctxpopup_place(inst->o_main, popup,
                                inst->cfg->memusage.o_gadget);
   evas_object_show(popup);

   return popup;
}

static void
_memusage_mouse_down_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Evas_Event_Mouse_Down *ev = event_data;
   Instance *inst = data;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (ev->button != 3)
     {
        if (inst->cfg->memusage.popup)
          elm_ctxpopup_dismiss(inst->cfg->memusage.popup);
        else
          {
             inst->cfg->memusage.popup = _memusage_popup_create(inst);
             _memusage_popup_update(inst);
          }
     }
   else
     {
        if (inst->cfg->memusage.popup)
          elm_ctxpopup_dismiss(inst->cfg->memusage.popup);
        if (!sysinfo_config) return;
        ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
        if (inst->cfg->esm != E_SYSINFO_MODULE_MEMUSAGE)
          memusage_configure(inst);
        else
          e_gadget_configure(inst->o_main);
     }
}

static void
_memusage_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Evas_Coord w, h;
   Instance *inst = data;

   edje_object_parts_extends_calc(elm_layout_edje_get(inst->cfg->memusage.o_gadget),
                                  0, 0, &w, &h);
   if (w < 1) w = 1;
   if (h < 1) h = 1;
   if (inst->cfg->esm == E_SYSINFO_MODULE_MEMUSAGE)
     evas_object_size_hint_aspect_set(inst->o_main,
                                      EVAS_ASPECT_CONTROL_BOTH, w, h);
   else
     evas_object_size_hint_aspect_set(inst->cfg->memusage.o_gadget,
                                      EVAS_ASPECT_CONTROL_BOTH, w, h);
}

static void
_memusage_cb_usage_check_main(void *data, Ecore_Thread *th)
{
   Thread_Config *thc = data;
   for (;; )
     {
        if (ecore_thread_check(th)) break;
#if defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__DragonFly__)
        _memusage_sysctl_getusage(&thc->mem_total, &thc->mem_used,
                                  &thc->mem_cached, &thc->mem_buffers, &thc->mem_shared,
                                  &thc->swp_total, &thc->swp_used);
#else
        _memusage_proc_getusage(&thc->mem_total, &thc->mem_used,
                                &thc->mem_cached, &thc->mem_buffers, &thc->mem_shared,
                                &thc->swp_total, &thc->swp_used);
#endif
        if (thc->mem_total > 0)
          thc->mem_percent = 100 * ((float)thc->mem_used / (float)thc->mem_total);
        if (thc->swp_total > 0)
          thc->swp_percent = 100 * ((float)thc->swp_used / (float)thc->swp_total);

        ecore_thread_feedback(th, NULL);
        if (ecore_thread_check(th)) break;
        e_powersave_sleeper_sleep(thc->sleeper, thc->interval);
        if (ecore_thread_check(th)) break;
     }
}

static void
_memusage_cb_usage_check_end(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Thread_Config *thc = data;
   e_powersave_sleeper_free(thc->sleeper);
   E_FREE(thc);
}

static void
_memusage_cb_usage_check_notify(void *data,
                                Ecore_Thread *th EINA_UNUSED,
                                void *msg EINA_UNUSED)
{
   Thread_Config *thc = data;

   if (!thc->inst->cfg) return;
   if (thc->inst->cfg->esm != E_SYSINFO_MODULE_MEMUSAGE &&
       thc->inst->cfg->esm != E_SYSINFO_MODULE_SYSINFO) return;

   thc->inst->cfg->memusage.mem_percent = thc->mem_percent;
   thc->inst->cfg->memusage.swp_percent = thc->swp_percent;
   thc->inst->cfg->memusage.mem_total = thc->mem_total;
   thc->inst->cfg->memusage.mem_used = thc->mem_used;
   thc->inst->cfg->memusage.mem_cached = thc->mem_cached;
   thc->inst->cfg->memusage.mem_buffers = thc->mem_buffers;
   thc->inst->cfg->memusage.mem_shared = thc->mem_shared;
   thc->inst->cfg->memusage.swp_total = thc->swp_total;
   thc->inst->cfg->memusage.swp_used = thc->swp_used;
   _memusage_face_update(thc->inst);
}

static Eina_Bool
_screensaver_on(void *data)
{
   Instance *inst = data;

   if (inst->cfg->memusage.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->memusage.usage_check_thread);
        inst->cfg->memusage.usage_check_thread = NULL;
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_screensaver_off(void *data)
{
   Instance *inst = data;

   _memusage_config_updated(inst);

   return ECORE_CALLBACK_RENEW;
}

void
_memusage_config_updated(Instance *inst)
{
   Thread_Config *thc;

   if (inst->cfg->id == -1)
     {
        inst->cfg->memusage.mem_percent = 75;
        inst->cfg->memusage.swp_percent = 30;
        _memusage_face_update(inst);
        return;
     }
   if (inst->cfg->memusage.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->memusage.usage_check_thread);
        inst->cfg->memusage.usage_check_thread = NULL;
     }
   thc = E_NEW(Thread_Config, 1);
   if (thc)
     {
        thc->inst = inst;
        thc->sleeper = e_powersave_sleeper_new();
        thc->interval = inst->cfg->memusage.poll_interval;
        thc->mem_percent = 0;
        thc->swp_percent = 0;
        inst->cfg->memusage.usage_check_thread =
          ecore_thread_feedback_run(_memusage_cb_usage_check_main,
                                    _memusage_cb_usage_check_notify,
                                    _memusage_cb_usage_check_end,
                                    _memusage_cb_usage_check_end, thc, EINA_TRUE);
     }
   e_config_save_queue();
}

static void
_memusage_removed_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_data)
{
   Instance *inst = data;
   Ecore_Event_Handler *handler;

   if (inst->o_main != event_data) return;

   if (inst->cfg->memusage.popup)
     E_FREE_FUNC(inst->cfg->memusage.popup, evas_object_del);
   if (inst->cfg->memusage.configure)
     E_FREE_FUNC(inst->cfg->memusage.configure, evas_object_del);
   evas_object_smart_callback_del_full(e_gadget_site_get(inst->o_main), "gadget_removed",
                                       _memusage_removed_cb, inst);
   evas_object_event_callback_del_full(inst->o_main, EVAS_CALLBACK_DEL,
                                       sysinfo_memusage_remove, data);
   EINA_LIST_FREE(inst->cfg->memusage.handlers, handler)
     ecore_event_handler_del(handler);
   if (inst->cfg->memusage.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->memusage.usage_check_thread);
        inst->cfg->memusage.usage_check_thread = NULL;
     }
   sysinfo_config->items = eina_list_remove(sysinfo_config->items, inst->cfg);
   if (inst->cfg->id >= 0)
     sysinfo_instances = eina_list_remove(sysinfo_instances, inst);
   E_FREE(inst->cfg);
   E_FREE(inst);
}

void
sysinfo_memusage_remove(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Instance *inst = data;
   Ecore_Event_Handler *handler;

   if (inst->cfg->memusage.popup)
     E_FREE_FUNC(inst->cfg->memusage.popup, evas_object_del);
   if (inst->cfg->memusage.configure)
     E_FREE_FUNC(inst->cfg->memusage.configure, evas_object_del);
   if (inst->cfg->memusage.usage_check_thread)
     {
        ecore_thread_cancel(inst->cfg->memusage.usage_check_thread);
        inst->cfg->memusage.usage_check_thread = NULL;
     }
   EINA_LIST_FREE(inst->cfg->memusage.handlers, handler)
     ecore_event_handler_del(handler);
}

static void
_memusage_created_cb(void *data, Evas_Object *obj, void *event_data EINA_UNUSED)
{
   Instance *inst = data;
   int orient = e_gadget_site_orient_get(e_gadget_site_get(inst->o_main));

   e_gadget_configure_cb_set(inst->o_main, _memusage_configure_cb);

   inst->cfg->memusage.o_gadget = elm_layout_add(inst->o_main);
   if (orient == E_GADGET_SITE_ORIENT_VERTICAL)
     e_theme_edje_object_set(inst->cfg->memusage.o_gadget,
                             "base/theme/gadget/memusage",
                             "e/gadget/memusage/main_vert");
   else
     e_theme_edje_object_set(inst->cfg->memusage.o_gadget,
                             "base/theme/gadget/memusage",
                             "e/gadget/memusage/main");

   E_EXPAND(inst->cfg->memusage.o_gadget);
   E_FILL(inst->cfg->memusage.o_gadget);
   elm_box_pack_end(inst->o_main, inst->cfg->memusage.o_gadget);
   evas_object_event_callback_add(inst->cfg->memusage.o_gadget,
                                  EVAS_CALLBACK_MOUSE_DOWN,
                                  _memusage_mouse_down_cb, inst);
   evas_object_event_callback_add(inst->cfg->memusage.o_gadget,
                                  EVAS_CALLBACK_RESIZE,
                                  _memusage_resize_cb, inst);
   evas_object_show(inst->cfg->memusage.o_gadget);
   evas_object_smart_callback_del_full(obj, "gadget_created",
                                       _memusage_created_cb, data);

   E_LIST_HANDLER_APPEND(inst->cfg->memusage.handlers, E_EVENT_SCREENSAVER_ON, _screensaver_on, inst);
   E_LIST_HANDLER_APPEND(inst->cfg->memusage.handlers, E_EVENT_SCREENSAVER_OFF, _screensaver_off, inst);

   _memusage_config_updated(inst);
}

Evas_Object *
sysinfo_memusage_create(Evas_Object *parent, Instance *inst)
{
   inst->cfg->memusage.mem_percent = 0;
   inst->cfg->memusage.swp_percent = 0;
   inst->cfg->memusage.mem_total = 0;
   inst->cfg->memusage.mem_used = 0;
   inst->cfg->memusage.mem_cached = 0;
   inst->cfg->memusage.mem_buffers = 0;
   inst->cfg->memusage.mem_shared = 0;
   inst->cfg->memusage.swp_total = 0;
   inst->cfg->memusage.swp_used = 0;
   inst->cfg->memusage.popup = NULL;
   inst->cfg->memusage.configure = NULL;
   inst->cfg->memusage.o_gadget = elm_layout_add(parent);
   e_theme_edje_object_set(inst->cfg->memusage.o_gadget,
                           "base/theme/gadget/memusage",
                           "e/gadget/memusage/main");
   E_EXPAND(inst->cfg->memusage.o_gadget);
   E_FILL(inst->cfg->memusage.o_gadget);
   evas_object_event_callback_add(inst->cfg->memusage.o_gadget,
                                  EVAS_CALLBACK_MOUSE_DOWN,
                                  _memusage_mouse_down_cb, inst);
   evas_object_event_callback_add(inst->cfg->memusage.o_gadget,
                                  EVAS_CALLBACK_RESIZE,
                                  _memusage_resize_cb, inst);
   evas_object_show(inst->cfg->memusage.o_gadget);

   E_LIST_HANDLER_APPEND(inst->cfg->memusage.handlers, E_EVENT_SCREENSAVER_ON, _screensaver_on, inst);
   E_LIST_HANDLER_APPEND(inst->cfg->memusage.handlers, E_EVENT_SCREENSAVER_OFF, _screensaver_off, inst);

   _memusage_config_updated(inst);

   return inst->cfg->memusage.o_gadget;
}

static Config_Item *
_conf_item_get(int *id)
{
   Config_Item *ci;
   Eina_List *l;

   if (*id > 0)
     {
        EINA_LIST_FOREACH(sysinfo_config->items, l, ci)
          if (*id == ci->id && ci->esm == E_SYSINFO_MODULE_MEMUSAGE) return ci;
     }

   ci = E_NEW(Config_Item, 1);

   if (*id != -1)
     ci->id = eina_list_count(sysinfo_config->items) + 1;
   else
     ci->id = -1;

   ci->esm = E_SYSINFO_MODULE_MEMUSAGE;
   ci->memusage.poll_interval = 32;
   ci->memusage.mem_percent = 0;
   ci->memusage.swp_percent = 0;
   ci->memusage.mem_total = 0;
   ci->memusage.mem_used = 0;
   ci->memusage.mem_cached = 0;
   ci->memusage.mem_buffers = 0;
   ci->memusage.mem_shared = 0;
   ci->memusage.swp_total = 0;
   ci->memusage.swp_used = 0;
   ci->memusage.popup = NULL;
   ci->memusage.configure = NULL;

   sysinfo_config->items = eina_list_append(sysinfo_config->items, ci);

   return ci;
}

Evas_Object *
memusage_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient EINA_UNUSED)
{
   Instance *inst;

   inst = E_NEW(Instance, 1);
   inst->cfg = _conf_item_get(id);
   *id = inst->cfg->id;
   inst->cfg->memusage.mem_percent = 0;
   inst->cfg->memusage.swp_percent = 0;
   inst->cfg->memusage.mem_total = 0;
   inst->cfg->memusage.mem_used = 0;
   inst->cfg->memusage.mem_cached = 0;
   inst->cfg->memusage.mem_buffers = 0;
   inst->cfg->memusage.mem_shared = 0;
   inst->cfg->memusage.swp_total = 0;
   inst->cfg->memusage.swp_used = 0;
   inst->cfg->memusage.popup = NULL;
   inst->cfg->memusage.configure = NULL;
   inst->o_main = elm_box_add(parent);
   evas_object_data_set(inst->o_main, "Instance", inst);
   evas_object_smart_callback_add(parent, "gadget_created",
                                  _memusage_created_cb, inst);
   evas_object_smart_callback_add(parent, "gadget_removed",
                                  _memusage_removed_cb, inst);
   evas_object_event_callback_add(inst->o_main, EVAS_CALLBACK_DEL,
                                  sysinfo_memusage_remove, inst);
   evas_object_show(inst->o_main);

   if (inst->cfg->id < 0) return inst->o_main;

   sysinfo_instances = eina_list_append(sysinfo_instances, inst);

   return inst->o_main;
}

