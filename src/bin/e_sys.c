#include "e.h"

#define ACTION_TIMEOUT 30.0

/* local subsystem functions */
static Eina_Bool _e_sys_cb_timer(void *data);
static Eina_Bool _e_sys_cb_exit(void *data, int type, void *event);
static void      _e_sys_cb_logout_logout(void *data, E_Dialog *dia);
static void      _e_sys_cb_logout_wait(void *data, E_Dialog *dia);
static void      _e_sys_cb_logout_abort(void *data, E_Dialog *dia);
static Eina_Bool _e_sys_cb_logout_timer(void *data);
static void      _e_sys_logout_after(void);
static void      _e_sys_logout_begin(E_Sys_Action a_after, Eina_Bool raw);
static void      _e_sys_current_action(void);
static void      _e_sys_action_failed(void);
static int       _e_sys_action_do(E_Sys_Action a, char *param, Eina_Bool raw);

static Ecore_Event_Handler *_e_sys_exe_exit_handler = NULL;
static Ecore_Exe *_e_sys_halt_check_exe = NULL;
static Ecore_Exe *_e_sys_reboot_check_exe = NULL;
static Ecore_Exe *_e_sys_suspend_check_exe = NULL;
static Ecore_Exe *_e_sys_hibernate_check_exe = NULL;
static int _e_sys_can_halt = 0;
static int _e_sys_can_reboot = 0;
static int _e_sys_can_suspend = 0;
static int _e_sys_can_hibernate = 0;

static E_Sys_Action _e_sys_action_current = E_SYS_NONE;
static E_Sys_Action _e_sys_action_after = E_SYS_NONE;
static Eina_Bool _e_sys_action_after_raw = EINA_FALSE;
static Ecore_Exe *_e_sys_exe = NULL;
static double _e_sys_begin_time = 0.0;
static double _e_sys_logout_begin_time = 0.0;
static Ecore_Timer *_e_sys_logout_timer = NULL;
static E_Dialog *_e_sys_logout_confirm_dialog = NULL;
static Ecore_Timer *_e_sys_susp_hib_check_timer = NULL;
static double _e_sys_susp_hib_check_last_tick = 0.0;
static Ecore_Timer *_e_sys_phantom_wake_check_timer = NULL;

static Ecore_Event_Handler *_e_sys_acpi_handler = NULL;

static void _e_sys_systemd_handle_inhibit(void);
static void _e_sys_systemd_poweroff(void);
static void _e_sys_systemd_reboot(void);
static void _e_sys_systemd_suspend(void);
static void _e_sys_systemd_hibernate(void);
static void _e_sys_systemd_exists_cb(void *data, const Eldbus_Message *m, Eldbus_Pending *p);

static Eina_Bool systemd_works = EINA_FALSE;
static int _e_sys_systemd_inhibit_fd = -1;

static const int E_LOGOUT_AUTO_TIME = 60.0;
static const int E_LOGOUT_WAIT_TIME = 3.0;

static Ecore_Timer *action_timeout = NULL;

static Eldbus_Proxy *login1_manger_proxy = NULL;

static int _e_sys_comp_waiting = 0;

static Ecore_Timer *_e_sys_screensaver_unignore_timer = NULL;

static double resume_backlight;

E_API int E_EVENT_SYS_SUSPEND = -1;
E_API int E_EVENT_SYS_HIBERNATE = -1;
E_API int E_EVENT_SYS_RESUME = -1;

static Eina_Bool
_e_sys_comp_done2_cb(void *data)
{
   e_sys_action_raw_do((E_Sys_Action)(long)data, NULL);
   return EINA_FALSE;
}

static void
_e_sys_comp_done_cb(void *data, Evas_Object *obj, const char *sig, const char *src)
{
   if (_e_sys_comp_waiting == 1) _e_sys_comp_waiting--;
   edje_object_signal_callback_del(obj, sig, src, _e_sys_comp_done_cb);
   if (_e_sys_screensaver_unignore_timer)
     {
        ecore_timer_del(_e_sys_screensaver_unignore_timer);
        _e_sys_screensaver_unignore_timer = NULL;
     }
   e_screensaver_ignore();
   ecore_evas_manual_render_set(e_comp->ee, EINA_TRUE);
#ifndef HAVE_WAYLAND_ONLY
   if (e_comp->comp_type == E_PIXMAP_TYPE_X)
     {
        ecore_x_screensaver_suspend();
        ecore_x_dpms_force(EINA_TRUE);
     }
#endif
#ifdef HAVE_WAYLAND
   if (e_comp->comp_type != E_PIXMAP_TYPE_X)
     {
        if (e_comp->screen && e_comp->screen->dpms)
          e_comp->screen->dpms(3);
     }
#endif
   edje_freeze();
   ecore_timer_add(0.5, _e_sys_comp_done2_cb, data);
   E_FREE_FUNC(action_timeout, ecore_timer_del);
}

static Eina_Bool
_e_sys_comp_action_timeout(void *data)
{
   const Eina_List *l;
   E_Zone *zone;
   E_Sys_Action a = (long)(intptr_t)data;
   const char *sig = NULL;

   if (_e_sys_comp_waiting == 1) _e_sys_comp_waiting--;
   switch (a)
     {
      case E_SYS_LOGOUT:
        sig = "e,state,sys,logout,done";
        break;
      case E_SYS_HALT:
        sig = "e,state,sys,halt,done";
        break;
      case E_SYS_REBOOT:
        sig = "e,state,sys,reboot,done";
        break;
      case E_SYS_SUSPEND:
        sig = "e,state,sys,suspend,done";
        break;
      case E_SYS_HIBERNATE:
        sig = "e,state,sys,hibernate,done";
        break;
      default:
        break;
     }
   E_FREE_FUNC(action_timeout, ecore_timer_del);
   if (sig)
     {
        EINA_LIST_FOREACH(e_comp->zones, l, zone)
          edje_object_signal_callback_del(zone->over, sig, "e", _e_sys_comp_done_cb);
     }
   e_sys_action_raw_do(a, NULL);
   return EINA_FALSE;
}

static void
_e_sys_comp_zones_fade(const char *sig, Eina_Bool out)
{
   const Eina_List *l;
   E_Zone *zone;
   EINA_LIST_FOREACH(e_comp->zones, l, zone)
     {
        e_zone_fade_handle(zone, out, 0.5);
        edje_object_signal_emit(zone->base, sig, "e");
        edje_object_signal_emit(zone->over, sig, "e");
     }
}

static void
_e_sys_comp_emit_cb_wait(E_Sys_Action a, const char *sig, const char *rep, Eina_Bool nocomp_push)
{
   E_Zone *zone;

   if (_e_sys_comp_waiting == 0) _e_sys_comp_waiting++;
   if (nocomp_push) e_comp_override_add();
   else e_comp_override_timed_pop();

   _e_sys_comp_zones_fade(sig, nocomp_push);
#ifndef HAVE_WAYLAND_ONLY
   if (e_comp->comp_type == E_PIXMAP_TYPE_X)
     _e_comp_x_screensaver_on();
#endif

   if (rep)
     {
        zone = eina_list_data_get(e_comp->zones);
        if (zone) edje_object_signal_callback_add(zone->over, rep, "e", _e_sys_comp_done_cb, (void *)(long)a);
        if (action_timeout) ecore_timer_del(action_timeout);
        action_timeout = ecore_timer_loop_add(ACTION_TIMEOUT, (Ecore_Task_Cb)_e_sys_comp_action_timeout, (intptr_t*)(long)a);
     }
}

static void
_e_sys_comp_suspend(void)
{
   resume_backlight = e_config->backlight.normal;
   _e_sys_comp_emit_cb_wait(E_SYS_SUSPEND, "e,state,sys,suspend", "e,state,sys,suspend,done", EINA_TRUE);
}

static void
_e_sys_comp_hibernate(void)
{
   resume_backlight = e_config->backlight.normal;
   _e_sys_comp_emit_cb_wait(E_SYS_HIBERNATE, "e,state,sys,hibernate", "e,state,sys,hibernate,done", EINA_TRUE);
}

static void
_e_sys_comp_reboot(void)
{
   _e_sys_comp_emit_cb_wait(E_SYS_REBOOT, "e,state,sys,reboot", "e,state,sys,reboot,done", EINA_TRUE);
}

static void
_e_sys_comp_shutdown(void)
{
   _e_sys_comp_emit_cb_wait(E_SYS_HALT, "e,state,sys,halt", "e,state,sys,halt,done", EINA_TRUE);
}

static void
_e_sys_comp_logout(void)
{
   _e_sys_comp_emit_cb_wait(E_SYS_LOGOUT, "e,state,sys,logout", "e,state,sys,logout,done", EINA_TRUE);
}

static Eina_Bool
_e_sys_screensaver_unignore_delay(void *data EINA_UNUSED)
{
   _e_sys_screensaver_unignore_timer = NULL;
   e_screensaver_unignore();
   return EINA_FALSE;
}

static Eina_Bool
_e_sys_phantom_wake_check_cb(void *data EINA_UNUSED)
{
   E_Action *act = e_action_find("suspend_smart");

   _e_sys_phantom_wake_check_timer = NULL;
   if ((e_acpi_lid_is_closed()) && (act))
     {
        ERR("Phantom wake/resume while lid still closed? Suspending again");
        if (act->func.go) act->func.go(E_OBJECT(e_zone_current_get()), NULL);
     }
   return EINA_FALSE;
}

static Eina_Bool
_e_sys_comp_resume2(void *data EINA_UNUSED)
{
   Eina_List *l;
   E_Zone *zone;

#ifndef HAVE_WAYLAND_ONLY
   if (e_comp->comp_type == E_PIXMAP_TYPE_X)
     _e_comp_x_screensaver_off();
#endif
   EINA_LIST_FOREACH(e_comp->zones, l, zone)
     e_backlight_level_set(zone, resume_backlight, -1.0);
   _e_sys_comp_zones_fade("e,state,sys,resume", EINA_FALSE);
   if (e_acpi_lid_is_closed())
     {
        if (_e_sys_phantom_wake_check_timer)
          ecore_timer_del(_e_sys_phantom_wake_check_timer);
        _e_sys_phantom_wake_check_timer =
          ecore_timer_add(1.0, _e_sys_phantom_wake_check_cb, NULL);
     }
   return EINA_FALSE;
}

static void
_e_sys_comp_resume(void)
{
   edje_thaw();
   ecore_evas_manual_render_set(e_comp->ee, EINA_FALSE);
   evas_damage_rectangle_add(e_comp->evas, 0, 0, e_comp->w, e_comp->h);
#ifndef HAVE_WAYLAND_ONLY
   if (e_comp->comp_type == E_PIXMAP_TYPE_X)
     {
        ecore_x_dpms_force(EINA_FALSE);
        ecore_x_screensaver_resume();
        ecore_x_screensaver_reset();
     }
#endif
#ifdef HAVE_WAYLAND
   if (e_comp->comp_type != E_PIXMAP_TYPE_X)
     {
        if (e_comp->screen && e_comp->screen->dpms)
          e_comp->screen->dpms(0);
     }
#endif
   e_screensaver_deactivate();
   if (_e_sys_screensaver_unignore_timer)
     ecore_timer_del(_e_sys_screensaver_unignore_timer);
   _e_sys_screensaver_unignore_timer =
     ecore_timer_add(0.3, _e_sys_screensaver_unignore_delay, NULL);
   ecore_timer_add(0.6, _e_sys_comp_resume2, NULL);
}

static void
_e_sys_systemd_signal_prepare_shutdown(void *data EINA_UNUSED, const Eldbus_Message *msg)
{
   EINA_SAFETY_ON_TRUE_RETURN(eldbus_message_error_get(msg, NULL, NULL));
   Eina_Bool b = EINA_FALSE;

   if (!eldbus_message_arguments_get(msg, "b", &b)) return;
   fprintf(stderr, "SSS: systemd said to prepare for shutdown! bool=%i @%1.8f\n", (int)b, ecore_time_get());
}

static void
_e_sys_systemd_signal_prepare_sleep(void *data EINA_UNUSED, const Eldbus_Message *msg)
{
   EINA_SAFETY_ON_TRUE_RETURN(eldbus_message_error_get(msg, NULL, NULL));
   Eina_Bool b = EINA_FALSE;

   if (!eldbus_message_arguments_get(msg, "b", &b)) return;
   // b == 1 -> suspending
   // b == 0 -> resuming
   fprintf(stderr, "SSS: systemd said to prepare for sleep! bool=%i @%1.8f\n", (int)b, ecore_time_get());
   if (b == EINA_FALSE)
     {
        ecore_event_add(E_EVENT_SYS_RESUME, NULL, NULL, NULL);
        _e_sys_comp_resume();
     }
}

/* externally accessible functions */
EINTERN int
e_sys_init(void)
{
   Eldbus_Connection *conn;
   Eldbus_Object *obj;

   conn = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SYSTEM);
   obj = eldbus_object_get(conn, "org.freedesktop.login1",
                           "/org/freedesktop/login1");
   login1_manger_proxy = eldbus_proxy_get(obj,
                                          "org.freedesktop.login1.Manager");
   eldbus_proxy_signal_handler_add(login1_manger_proxy, "PrepareForShutdown",
                                   _e_sys_systemd_signal_prepare_shutdown,
                                   NULL);
   eldbus_proxy_signal_handler_add(login1_manger_proxy, "PrepareForSleep",
                                   _e_sys_systemd_signal_prepare_sleep,
                                   NULL);
   eldbus_name_owner_get(conn, "org.freedesktop.login1",
                         _e_sys_systemd_exists_cb, NULL);
   _e_sys_systemd_handle_inhibit();

   E_EVENT_SYS_SUSPEND = ecore_event_type_new();
   E_EVENT_SYS_HIBERNATE = ecore_event_type_new();
   E_EVENT_SYS_RESUME = ecore_event_type_new();
   /* this is not optimal - but it does work cleanly */
   _e_sys_exe_exit_handler = ecore_event_handler_add(ECORE_EXE_EVENT_DEL,
                                                     _e_sys_cb_exit, NULL);
   return 1;
}

EINTERN int
e_sys_shutdown(void)
{
   if (_e_sys_screensaver_unignore_timer)
     ecore_timer_del(_e_sys_screensaver_unignore_timer);
   if (_e_sys_acpi_handler)
     ecore_event_handler_del(_e_sys_acpi_handler);
   if (_e_sys_exe_exit_handler)
     ecore_event_handler_del(_e_sys_exe_exit_handler);
   _e_sys_screensaver_unignore_timer = NULL;
   _e_sys_acpi_handler = NULL;
   _e_sys_exe_exit_handler = NULL;
   _e_sys_halt_check_exe = NULL;
   _e_sys_reboot_check_exe = NULL;
   _e_sys_suspend_check_exe = NULL;
   _e_sys_hibernate_check_exe = NULL;
   if (login1_manger_proxy)
     {
         Eldbus_Connection *conn;
         Eldbus_Object *obj;

         obj = eldbus_proxy_object_get(login1_manger_proxy);
         conn = eldbus_object_connection_get(obj);
         eldbus_proxy_unref(login1_manger_proxy);
         eldbus_object_unref(obj);
         eldbus_connection_unref(conn);
         login1_manger_proxy = NULL;
     }
   if (_e_sys_systemd_inhibit_fd >= 0)
     {
        close(_e_sys_systemd_inhibit_fd);
        _e_sys_systemd_inhibit_fd = -1;
     }
   return 1;
}

E_API int
e_sys_action_possible_get(E_Sys_Action a)
{
   switch (a)
     {
      case E_SYS_EXIT:
      case E_SYS_RESTART:
      case E_SYS_EXIT_NOW:
        return 1;

      case E_SYS_LOGOUT:
        return 1;

      case E_SYS_HALT:
      case E_SYS_HALT_NOW:
        return _e_sys_can_halt;

      case E_SYS_REBOOT:
        return _e_sys_can_reboot;

      case E_SYS_SUSPEND:
        return _e_sys_can_suspend;

      case E_SYS_HIBERNATE:
        return _e_sys_can_hibernate;

      default:
        return 0;
     }
   return 0;
}

E_API int
e_sys_action_do(E_Sys_Action a, char *param)
{
   int ret = 0;

   if (_e_sys_action_current != E_SYS_NONE)
     {
        _e_sys_current_action();
        return 0;
     }
   e_config_save_flush();
   switch (a)
     {
      case E_SYS_EXIT:
      case E_SYS_RESTART:
      case E_SYS_EXIT_NOW:
      case E_SYS_LOGOUT:
      case E_SYS_SUSPEND:
      case E_SYS_HIBERNATE:
      case E_SYS_HALT_NOW:
        ret = _e_sys_action_do(a, param, EINA_FALSE);
        break;

      case E_SYS_HALT:
      case E_SYS_REBOOT:
        if (!e_util_immortal_check())
          ret = _e_sys_action_do(a, param, EINA_FALSE);
        break;

      default:
        break;
     }

   if (ret) _e_sys_action_current = a;
   else _e_sys_action_current = E_SYS_NONE;

   return ret;
}

E_API int
e_sys_action_raw_do(E_Sys_Action a, char *param)
{
   int ret = 0;

   e_config_save_flush();
   if (_e_sys_action_current != E_SYS_NONE)
     {
        _e_sys_current_action();
        return 0;
     }
   ret = _e_sys_action_do(a, param, EINA_TRUE);

   if (ret) _e_sys_action_current = a;
   else _e_sys_action_current = E_SYS_NONE;

   return ret;
}

static Eina_List *extra_actions = NULL;

E_API E_Sys_Con_Action *
e_sys_con_extra_action_register(const char *label,
                                const char *icon_group,
                                const char *button_name,
                                void (*func)(void *data),
                                const void *data)
{
   E_Sys_Con_Action *sca;

   sca = E_NEW(E_Sys_Con_Action, 1);
   if (label)
     sca->label = eina_stringshare_add(label);
   if (icon_group)
     sca->icon_group = eina_stringshare_add(icon_group);
   if (button_name)
     sca->button_name = eina_stringshare_add(button_name);
   sca->func = func;
   sca->data = data;
   extra_actions = eina_list_append(extra_actions, sca);
   return sca;
}

E_API void
e_sys_con_extra_action_unregister(E_Sys_Con_Action *sca)
{
   extra_actions = eina_list_remove(extra_actions, sca);
   if (sca->label) eina_stringshare_del(sca->label);
   if (sca->icon_group) eina_stringshare_del(sca->icon_group);
   if (sca->button_name) eina_stringshare_del(sca->button_name);
   free(sca);
}

E_API const Eina_List *
e_sys_con_extra_action_list_get(void)
{
   return extra_actions;
}

static void
_e_sys_systemd_inhibit_cb(void *data EINA_UNUSED, const Eldbus_Message *m, Eldbus_Pending *p EINA_UNUSED)
{
   int fd = -1;
   if (eldbus_message_error_get(m, NULL, NULL)) return;
   if (!eldbus_message_arguments_get(m, "h", &fd))
     _e_sys_systemd_inhibit_fd = fd;
}

static void
_e_sys_systemd_handle_inhibit(void)
{
   Eldbus_Message *m;
   
   if (!login1_manger_proxy) return;
   if (!(m = eldbus_proxy_method_call_new(login1_manger_proxy, "Inhibit")))
     return;
   eldbus_message_arguments_append
     (m, "ssss",
         "handle-power-key:"
         "handle-suspend-key:"
         "handle-hibernate-key:"
         "handle-lid-switch", // what
         "Enlightenment", // who (string)
         "Normal Execution", // why (string)
         "block");
   eldbus_proxy_send(login1_manger_proxy, m, _e_sys_systemd_inhibit_cb, NULL, -1);
}

static void
_e_sys_systemd_check_cb(void *data, const Eldbus_Message *m, Eldbus_Pending *p EINA_UNUSED)
{
   int *dest = data;
   char *s = NULL;
   if (!eldbus_message_arguments_get(m, "s", &s)) return;
   if (!s) return;
   if (!strcmp(s, "yes")) *dest = 1;
   else *dest = 0;
}

static void
_e_sys_systemd_check(void)
{
   if (!login1_manger_proxy) return;
   if (!eldbus_proxy_call(login1_manger_proxy, "CanPowerOff",
                          _e_sys_systemd_check_cb, &_e_sys_can_halt, -1, ""))
     return;
   if (!eldbus_proxy_call(login1_manger_proxy, "CanReboot",
                          _e_sys_systemd_check_cb, &_e_sys_can_reboot, -1, ""))
     return;
   if (!eldbus_proxy_call(login1_manger_proxy, "CanSuspend",
                          _e_sys_systemd_check_cb, &_e_sys_can_suspend, -1, ""))
     return;
   if (!eldbus_proxy_call(login1_manger_proxy, "CanHibernate",
                          _e_sys_systemd_check_cb, &_e_sys_can_hibernate, -1, ""))
     return;
}

static void
_e_sys_systemd_exists_cb(void *data EINA_UNUSED, const Eldbus_Message *m, Eldbus_Pending *p EINA_UNUSED)
{
   const char *id = NULL;
   
   if (eldbus_message_error_get(m, NULL, NULL)) goto fail;
   if (!eldbus_message_arguments_get(m, "s", &id)) goto fail;
   if ((!id) || (id[0] != ':')) goto fail;
   systemd_works = EINA_TRUE;
   _e_sys_systemd_check();
   return;
fail:
   systemd_works = EINA_FALSE;
   /* delay this for 1.0 seconds while the rest of e starts up */
   ecore_timer_loop_add(1.0, _e_sys_cb_timer, NULL);
}

static void
_e_sys_systemd_poweroff(void)
{
   eldbus_proxy_call(login1_manger_proxy, "PowerOff", NULL, NULL, -1, "b", 0);
}

static void
_e_sys_systemd_reboot(void)
{
   eldbus_proxy_call(login1_manger_proxy, "Reboot", NULL, NULL, -1, "b", 0);
}

static void
_e_sys_systemd_suspend(void)
{
   eldbus_proxy_call(login1_manger_proxy, "Suspend", NULL, NULL, -1, "b", 0);
}

static void
_e_sys_systemd_hibernate(void)
{
   eldbus_proxy_call(login1_manger_proxy, "Hibernate", NULL, NULL, -1, "b", 0);
}

static Eina_Bool
_e_sys_resume_delay(void *d EINA_UNUSED)
{
   ecore_event_add(E_EVENT_SYS_RESUME, NULL, NULL, NULL);
   _e_sys_comp_resume();
   return EINA_FALSE;
}

static Eina_Bool
_e_sys_susp_hib_check_timer_cb(void *data EINA_UNUSED)
{
   double t = ecore_time_unix_get();

   if ((t - _e_sys_susp_hib_check_last_tick) > 0.5)
     {
        _e_sys_susp_hib_check_timer = NULL;
        ecore_timer_add(0.2, _e_sys_resume_delay, NULL);
        return EINA_FALSE;
     }
   _e_sys_susp_hib_check_last_tick = t;
   return EINA_TRUE;
}

static void
_e_sys_susp_hib_check(void)
{
   if (_e_sys_susp_hib_check_timer)
     ecore_timer_del(_e_sys_susp_hib_check_timer);
   _e_sys_susp_hib_check_last_tick = ecore_time_unix_get();
   _e_sys_susp_hib_check_timer =
     ecore_timer_loop_add(0.1, _e_sys_susp_hib_check_timer_cb, NULL);
}

/* local subsystem functions */
static Eina_Bool
_e_sys_cb_timer(void *data EINA_UNUSED)
{
   /* exec out sys helper and ask it to test if we are allowed to do these
    * things
    */
   char buf[4096];

   e_init_status_set(_("Checking System Permissions"));
   snprintf(buf, sizeof(buf),
            "%s/enlightenment/utils/enlightenment_sys -t halt",
            e_prefix_lib_get());
   _e_sys_halt_check_exe = ecore_exe_run(buf, NULL);
   snprintf(buf, sizeof(buf),
            "%s/enlightenment/utils/enlightenment_sys -t reboot",
            e_prefix_lib_get());
   _e_sys_reboot_check_exe = ecore_exe_run(buf, NULL);
   snprintf(buf, sizeof(buf),
            "%s/enlightenment/utils/enlightenment_sys -t suspend",
            e_prefix_lib_get());
   _e_sys_suspend_check_exe = ecore_exe_run(buf, NULL);
   snprintf(buf, sizeof(buf),
            "%s/enlightenment/utils/enlightenment_sys -t hibernate",
            e_prefix_lib_get());
   _e_sys_hibernate_check_exe = ecore_exe_run(buf, NULL);
   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_e_sys_cb_exit(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Exe_Event_Del *ev;

   ev = event;
   if ((_e_sys_exe) && (ev->exe == _e_sys_exe))
     {
        if (ev->exit_code != 0) _e_sys_action_failed();
        _e_sys_action_current = E_SYS_NONE;
        _e_sys_exe = NULL;
        return ECORE_CALLBACK_RENEW;
     }
   if ((_e_sys_halt_check_exe) && (ev->exe == _e_sys_halt_check_exe))
     {
        e_init_status_set(_("System Check Done"));
        /* exit_code: 0 == OK, 5 == suid root removed, 7 == group id error
         * 10 == permission denied, 20 == action undefined */
        if (ev->exit_code == 0)
          {
             _e_sys_can_halt = 1;
             _e_sys_halt_check_exe = NULL;
          }
     }
   else if ((_e_sys_reboot_check_exe) && (ev->exe == _e_sys_reboot_check_exe))
     {
        e_init_status_set(_("System Check Done"));
        if (ev->exit_code == 0)
          {
             _e_sys_can_reboot = 1;
             _e_sys_reboot_check_exe = NULL;
          }
     }
   else if ((_e_sys_suspend_check_exe) && (ev->exe == _e_sys_suspend_check_exe))
     {
        e_init_status_set(_("System Check Done"));
        if (ev->exit_code == 0)
          {
             _e_sys_can_suspend = 1;
             _e_sys_suspend_check_exe = NULL;
          }
     }
   else if ((_e_sys_hibernate_check_exe) && (ev->exe == _e_sys_hibernate_check_exe))
     {
        e_init_status_set(_("System Check Done"));
        if (ev->exit_code == 0)
          {
             _e_sys_can_hibernate = 1;
             _e_sys_hibernate_check_exe = NULL;
          }
     }
   return ECORE_CALLBACK_RENEW;
}

static void
_e_sys_cb_logout_logout(void *data EINA_UNUSED, E_Dialog *dia)
{
   if (_e_sys_logout_timer)
     {
        ecore_timer_del(_e_sys_logout_timer);
        _e_sys_logout_timer = NULL;
     }
   _e_sys_logout_begin_time = 0.0;
   _e_sys_logout_after();
   e_object_del(E_OBJECT(dia));
   _e_sys_logout_confirm_dialog = NULL;
}

static void
_e_sys_cb_logout_wait(void *data EINA_UNUSED, E_Dialog *dia)
{
   if (_e_sys_logout_timer) ecore_timer_del(_e_sys_logout_timer);
   _e_sys_logout_timer = ecore_timer_loop_add(0.5, _e_sys_cb_logout_timer, NULL);
   _e_sys_logout_begin_time = ecore_time_get();
   e_object_del(E_OBJECT(dia));
   _e_sys_logout_confirm_dialog = NULL;
}

static void
_e_sys_cb_logout_abort(void *data EINA_UNUSED, E_Dialog *dia)
{
   if (_e_sys_logout_timer)
     {
        ecore_timer_del(_e_sys_logout_timer);
        _e_sys_logout_timer = NULL;
     }
   _e_sys_logout_begin_time = 0.0;
   e_object_del(E_OBJECT(dia));
   _e_sys_logout_confirm_dialog = NULL;
   _e_sys_action_current = E_SYS_NONE;
   _e_sys_action_after = E_SYS_NONE;
   _e_sys_action_after_raw = EINA_FALSE;
}

static void
_e_sys_logout_confirm_dialog_update(int remaining)
{
   char txt[4096];

   if (!_e_sys_logout_confirm_dialog)
     {
        fputs("ERROR: updating logout confirm dialog, but none exists!\n",
              stderr);
        return;
     }

   snprintf(txt, sizeof(txt),
            _("Logout is taking too long.<ps/>"
              "Some applications refuse to close.<ps/>"
              "Do you want to finish the logout<ps/>"
              "anyway without closing these<ps/>"
              "applications first?<ps/><ps/>"
              "Auto logout in %d seconds."), remaining);

   e_dialog_text_set(_e_sys_logout_confirm_dialog, txt);
}

static Eina_Bool
_e_sys_cb_logout_timer(void *data EINA_UNUSED)
{
   E_Client *ec;
   int pending = 0;

   E_CLIENT_FOREACH(ec)
     {
        if (e_client_util_ignored_get(ec)) continue;
        if (!ec->internal) pending++;
     }
   if (pending == 0) goto after;
   else if (_e_sys_logout_confirm_dialog)
     {
        int remaining = E_LOGOUT_AUTO_TIME -
          round(ecore_loop_time_get() - _e_sys_logout_begin_time);
        /* it has taken 60 (E_LOGOUT_AUTO_TIME) seconds of waiting the
         * confirm dialog and we still have apps that will not go
         * away. Do the action as user may be far away or forgot it.
         *
         * NOTE: this is the behavior for many operating systems and I
         *       guess the reason is people that hit "shutdown" and
         *       put their laptops in their backpacks in the hope
         *       everything will be turned off properly.
         */
        if (remaining > 0)
          {
             _e_sys_logout_confirm_dialog_update(remaining);
             return ECORE_CALLBACK_RENEW;
          }
        else
          {
             _e_sys_cb_logout_logout(NULL, _e_sys_logout_confirm_dialog);
             return ECORE_CALLBACK_CANCEL;
          }
     }
   else
     {
        // it has taken E_LOGOUT_WAIT_TIME seconds of waiting and we still
        // have apps that will not go away
        double now = ecore_loop_time_get();
        if ((now - _e_sys_logout_begin_time) > E_LOGOUT_WAIT_TIME)
          {
             E_Dialog *dia;

             dia = e_dialog_new(NULL, "E", "_sys_error_logout_slow");
             if (dia)
               {
                  _e_sys_logout_confirm_dialog = dia;
                  e_dialog_title_set(dia, _("Logout problems"));
                  e_dialog_icon_set(dia, "system-log-out", 64);
                  _e_sys_logout_confirm_dialog_update(E_LOGOUT_AUTO_TIME);
                  e_dialog_button_add(dia, _("Logout now"), NULL,
                                      _e_sys_cb_logout_logout, NULL);
                  e_dialog_button_add(dia, _("Wait longer"), NULL,
                                      _e_sys_cb_logout_wait, NULL);
                  e_dialog_button_add(dia, _("Cancel Logout"), NULL,
                                      _e_sys_cb_logout_abort, NULL);
                  e_dialog_button_focus_num(dia, 1);
                  elm_win_center(dia->win, 1, 1);
                  e_win_no_remember_set(dia->win, 1);
                  e_dialog_show(dia);
                  _e_sys_logout_begin_time = now;
                  resume_backlight = e_config->backlight.normal;
               }
             _e_sys_comp_resume();
             return ECORE_CALLBACK_RENEW;
          }
     }
   return ECORE_CALLBACK_RENEW;
after:
   switch (_e_sys_action_after)
     {
      case E_SYS_EXIT:
        _e_sys_comp_logout();
        break;
      case E_SYS_HALT:
      case E_SYS_HALT_NOW:
        _e_sys_comp_shutdown();
        break;
      case E_SYS_REBOOT:
        _e_sys_comp_reboot();
        break;
      default: break;
     }
   _e_sys_logout_timer = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static void
_e_sys_logout_after(void)
{
   _e_sys_action_current = _e_sys_action_after;
   _e_sys_action_do(_e_sys_action_after, NULL, _e_sys_action_after_raw);
   _e_sys_action_after = E_SYS_NONE;
   _e_sys_action_after_raw = EINA_FALSE;
}

static void
_e_sys_logout_begin(E_Sys_Action a_after, Eina_Bool raw)
{
   const Eina_List *l, *ll;
   E_Client *ec;

   stopping = 1;
   /* start logout - at end do the a_after action */
   _e_sys_action_after = a_after;
   _e_sys_action_after_raw = raw;
   EINA_LIST_FOREACH_SAFE(e_comp->clients, l, ll, ec)
     {
        e_client_act_close_begin(ec);
     }
   /* and poll to see if all pending windows are gone yet every 0.5 sec */
   _e_sys_logout_begin_time = ecore_time_get();
   if (_e_sys_logout_timer) ecore_timer_del(_e_sys_logout_timer);
   _e_sys_logout_timer = ecore_timer_loop_add(0.5, _e_sys_cb_logout_timer, NULL);
}

static void
_e_sys_current_action(void)
{
   /* display dialog that currently an action is in progress */
   E_Dialog *dia;

   dia = e_dialog_new(NULL,
                      "E", "_sys_error_action_busy");
   if (!dia) return;

   e_dialog_title_set(dia, _("Enlightenment is busy with another request"));
   e_dialog_icon_set(dia, "enlightenment/sys", 64);
   switch (_e_sys_action_current)
     {
      case E_SYS_LOGOUT:
        e_dialog_text_set(dia, _("Logging out.<ps/>"
                                 "You cannot perform other system actions<ps/>"
                                 "once a logout has begun."));
        break;

      case E_SYS_HALT:
      case E_SYS_HALT_NOW:
        e_dialog_text_set(dia, _("Powering off.<ps/>"
                                 "You cannot perform any other system actions<ps/>"
                                 "once a shutdown has been started."));
        break;

      case E_SYS_REBOOT:
        e_dialog_text_set(dia, _("Resetting.<ps/>"
                                 "You cannot perform any other system actions<ps/>"
                                 "once a reboot has begun."));
        break;

      case E_SYS_SUSPEND:
        e_dialog_text_set(dia, _("Suspending.<ps/>"
                                 "You cannot perform any other system actions<ps/>"
                                 "until suspend is complete."));
        break;

      case E_SYS_HIBERNATE:
        e_dialog_text_set(dia, _("Hibernating.<ps/>"
                                 "You cannot perform any other system actions<ps/>"
                                 "until hibernation is complete."));
        break;

      default:
        e_dialog_text_set(dia, _("EEK! This should not happen"));
        break;
     }
   e_dialog_button_add(dia, _("OK"), NULL, NULL, NULL);
   e_dialog_button_focus_num(dia, 0);
   elm_win_center(dia->win, 1, 1);
   e_win_no_remember_set(dia->win, 1);
   e_dialog_show(dia);
}

static void
_e_sys_action_failed(void)
{
   /* display dialog that the current action failed */
   E_Dialog *dia;

   dia = e_dialog_new(NULL,
                      "E", "_sys_error_action_failed");
   if (!dia) return;

   e_dialog_title_set(dia, _("Enlightenment is busy with another request"));
   e_dialog_icon_set(dia, "enlightenment/sys", 64);
   switch (_e_sys_action_current)
     {
      case E_SYS_HALT:
      case E_SYS_HALT_NOW:
        e_dialog_text_set(dia, _("Power off failed."));
        break;

      case E_SYS_REBOOT:
        e_dialog_text_set(dia, _("Reset failed."));
        break;

      case E_SYS_SUSPEND:
        e_dialog_text_set(dia, _("Suspend failed."));
        break;

      case E_SYS_HIBERNATE:
        e_dialog_text_set(dia, _("Hibernate failed."));
        break;

      default:
        e_dialog_text_set(dia, _("EEK! This should not happen"));
        break;
     }
   e_dialog_button_add(dia, _("OK"), NULL, NULL, NULL);
   e_dialog_button_focus_num(dia, 0);
   elm_win_center(dia->win, 1, 1);
   e_win_no_remember_set(dia->win, 1);
   e_dialog_show(dia);
}

static Eina_Bool
_e_sys_cb_acpi_event(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Acpi *ev = event;

   if (e_powersave_mode_get() == E_POWERSAVE_MODE_FREEZE)
     {
        if (((ev->type == E_ACPI_TYPE_LID) &&
             (ev->status == E_ACPI_LID_OPEN)) ||
            (ev->type == E_ACPI_TYPE_POWER) ||
            (ev->type == E_ACPI_TYPE_SLEEP))
          {
             if (_e_sys_acpi_handler)
               {
                  ecore_event_handler_del(_e_sys_acpi_handler);
                  _e_sys_acpi_handler = NULL;
               }
             e_powersave_mode_unforce();
             ecore_timer_add(1.0, _e_sys_resume_delay, NULL);
             // XXX: need some way of, at the system level, restoring
             // system and devices back to running normally
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static int
_e_sys_action_do(E_Sys_Action a, char *param EINA_UNUSED, Eina_Bool raw)
{
   char buf[PATH_MAX];
   int ret = 0;

   switch (a)
     {
      case E_SYS_EXIT:
        // XXX TODO: check for e_fm_op_registry entries and confirm
        if (!e_util_immortal_check())
          ecore_main_loop_quit();
        else
          return 0;
        break;

      case E_SYS_RESTART:
        // XXX TODO: check for e_fm_op_registry entries and confirm
        // FIXME: we don't share out immortal info to restarted e. :(
//	if (!e_util_immortal_check())
      {
         restart = 1;
         ecore_main_loop_quit();
      }
//        else
//          return 0;
      break;

      case E_SYS_EXIT_NOW:
        exit(0);
        break;

      case E_SYS_LOGOUT:
        // XXX TODO: check for e_fm_op_registry entries and confirm
        if (raw)
          {
             _e_sys_logout_after();
          }
        else if (!_e_sys_comp_waiting)
          {
             _e_sys_logout_begin(E_SYS_EXIT, raw);
          }
        break;

      case E_SYS_HALT:
      case E_SYS_HALT_NOW:
        /* shutdown -h now */
        if (e_util_immortal_check()) return 0;
        snprintf(buf, sizeof(buf),
                 "%s/enlightenment/utils/enlightenment_sys halt",
                 e_prefix_lib_get());
        if (_e_sys_exe)
          {
             if ((ecore_time_get() - _e_sys_begin_time) > 2.0)
               _e_sys_current_action();
             return 0;
          }
        else if (!_e_sys_comp_waiting)
          {
             if (raw)
               {
                  _e_sys_begin_time = ecore_time_get();
                  if (systemd_works)
                    _e_sys_systemd_poweroff();
                  else
                    {
                       _e_sys_exe = ecore_exe_run(buf, NULL);
                       ret = 1;
                    }
               }
             else
               {
                  ret = 0;
                  _e_sys_begin_time = ecore_time_get();
                  _e_sys_logout_begin(a, EINA_TRUE);
               }
             /* FIXME: display halt status */
          }
        break;

      case E_SYS_REBOOT:
        /* shutdown -r now */
        if (e_util_immortal_check()) return 0;
        snprintf(buf, sizeof(buf),
                 "%s/enlightenment/utils/enlightenment_sys reboot",
                 e_prefix_lib_get());
        if (_e_sys_exe)
          {
             if ((ecore_time_get() - _e_sys_begin_time) > 2.0)
               _e_sys_current_action();
             return 0;
          }
        else if (!_e_sys_comp_waiting)
          {
             if (raw)
               {
                  _e_sys_begin_time = ecore_time_get();
                  if (systemd_works)
                    _e_sys_systemd_reboot();
                  else
                    {
                       _e_sys_exe = ecore_exe_run(buf, NULL);
                       ret = 1;
                    }
               }
             else
               {
                  ret = 0;
                  _e_sys_begin_time = ecore_time_get();
                  _e_sys_logout_begin(a, EINA_TRUE);
               }
             /* FIXME: display reboot status */
          }
        break;

      case E_SYS_SUSPEND:
        /* /etc/acpi/sleep.sh force */
        snprintf(buf, sizeof(buf),
                 "%s/enlightenment/utils/enlightenment_sys suspend",
                 e_prefix_lib_get());
        if (_e_sys_exe)
          {
             if ((ecore_time_get() - _e_sys_begin_time) > 2.0)
               _e_sys_current_action();
             return 0;
          }
        else if (!_e_sys_comp_waiting)
          {
             if (_e_sys_phantom_wake_check_timer)
               ecore_timer_del(_e_sys_phantom_wake_check_timer);
             _e_sys_phantom_wake_check_timer = NULL;
             if (raw)
               {
                  if (e_config->desklock_on_suspend)
                  // XXX: this desklock - ensure its instant
                    e_desklock_show(EINA_TRUE);
                  _e_sys_begin_time = ecore_time_get();
                  if (e_config->suspend_connected_standby == 0)
                    {
                       _e_sys_susp_hib_check();
                       if (systemd_works)
                         _e_sys_systemd_suspend();
                       else
                         {
                            _e_sys_exe = ecore_exe_run(buf, NULL);
                            ret = 1;
                         }
                    }
                  else
                    {
                       if (_e_sys_acpi_handler)
                         ecore_event_handler_del(_e_sys_acpi_handler);
                       _e_sys_acpi_handler =
                         ecore_event_handler_add(E_EVENT_ACPI,
                                                 _e_sys_cb_acpi_event,
                                                 NULL);
                       e_powersave_mode_force(E_POWERSAVE_MODE_FREEZE);
                       // XXX: need some system way of forcing the system
                       // into a very lowe power level with as many
                       // devices suspended as possible. below is a simple
                       // "freeze the cpu/kernel" which is not what we
                       // want actually
                       // ecore_exe_run("sleep 2 && echo freeze | sudo tee /sys/power/state", NULL);
                    }
               }
             else
               {
                  ecore_event_add(E_EVENT_SYS_SUSPEND, NULL, NULL, NULL);
                  _e_sys_comp_suspend();
                  return 0;
               }
             /* FIXME: display suspend status */
          }
        break;

      case E_SYS_HIBERNATE:
        /* /etc/acpi/hibernate.sh force */
        snprintf(buf, sizeof(buf),
                 "%s/enlightenment/utils/enlightenment_sys hibernate",
                 e_prefix_lib_get());
        if (_e_sys_exe)
          {
             if ((ecore_time_get() - _e_sys_begin_time) > 2.0)
               _e_sys_current_action();
             return 0;
          }
        else if (!_e_sys_comp_waiting)
          {
             if (_e_sys_phantom_wake_check_timer)
               ecore_timer_del(_e_sys_phantom_wake_check_timer);
             _e_sys_phantom_wake_check_timer = NULL;
             if (raw)
               {
                  _e_sys_susp_hib_check();
                  if (e_config->desklock_on_suspend)
                  // XXX: this desklock - ensure its instant
                    e_desklock_show(EINA_TRUE);
                  _e_sys_begin_time = ecore_time_get();
                  if (systemd_works)
                    _e_sys_systemd_hibernate();
                  else
                    {
                       _e_sys_exe = ecore_exe_run(buf, NULL);
                       ret = 1;
                    }
               }
             else
               {
                  ecore_event_add(E_EVENT_SYS_HIBERNATE, NULL, NULL, NULL);
                  _e_sys_comp_hibernate();
                  return 0;
               }
             /* FIXME: display hibernate status */
          }
        break;

      default:
        return 0;
     }
   return ret;
}
