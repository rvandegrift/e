#include "pager.h"

/* actual module specifics */
typedef struct _Instance    Instance;
typedef struct _Pager       Pager;
typedef struct _Pager_Desk  Pager_Desk;
typedef struct _Pager_Win   Pager_Win;
typedef struct _Pager_Popup Pager_Popup;

struct _Instance
{
   Evas_Object *o_pager;     /* table */
   Pager       *pager;
   Pager_Popup *popup;
};

struct _Pager
{
   Instance     *inst;
   Pager_Popup  *popup;
   Evas_Object  *o_table;
   E_Zone       *zone;
   int           xnum, ynum;
   Eina_List    *desks;
   Pager_Desk   *active_pd;
   unsigned char dragging : 1;
   unsigned char just_dragged : 1;
   E_Client     *active_drag_client;
   Ecore_Job    *recalc;
   Eina_Bool     invert : 1;
   Eina_Bool     plain : 1;
};

struct _Pager_Desk
{
   Pager       *pager;
   E_Desk      *desk;
   Eina_List   *wins;
   Evas_Object *o_desk;
   Evas_Object *o_layout;
   Evas_Object *drop_handler;
   int          xpos, ypos, urgent;
   int          current : 1;
   struct
   {
      Pager        *from_pager;
      unsigned char in_pager : 1;
      unsigned char start : 1;
      int           x, y, dx, dy, button;
   } drag;
};

struct _Pager_Win
{
   E_Client     *client;
   Pager_Desk   *desk;
   Evas_Object  *o_window;
   Evas_Object  *o_mirror;
   Evas_Object  *o_icon;
   unsigned char skip_winlist : 1;
   struct
   {
      Pager        *from_pager;
      unsigned char start : 1;
      unsigned char in_pager : 1;
      unsigned char desktop  : 1;
      int           x, y, dx, dy, button;
   } drag;
};

struct _Pager_Popup
{
   Evas_Object  *popup;
   Evas_Object  *o_bg;
   Pager        *pager;
   Ecore_Timer  *timer;
   unsigned char urgent : 1;
};

static void         _pager_cb_mirror_add(Pager_Desk *pd, Evas_Object *obj, Evas_Object *mirror);

static void         _pager_cb_obj_show(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED);
static void         _pager_cb_obj_hide(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED);
static void         _button_cb_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info);
static Evas_Object *_pager_gadget_configure(Evas_Object *g);
static Eina_Bool    _pager_cb_event_powersave_change(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool    _pager_cb_event_desk_show(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool    _pager_cb_event_desk_name_change(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool    _pager_cb_event_compositor_resize(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool    _pager_cb_event_client_resize(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool    _pager_cb_event_client_move(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool    _pager_cb_event_client_add(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool    _pager_cb_event_client_remove(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool    _pager_cb_event_client_iconify(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool    _pager_cb_event_client_uniconify(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool    _pager_cb_event_client_stick(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool    _pager_cb_event_client_unstick(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool    _pager_cb_event_client_desk_set(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool    _pager_cb_event_client_stack(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool    _pager_cb_event_client_icon_change(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool    _pager_cb_event_client_focus_in(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool    _pager_cb_event_client_focus_out(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool    _pager_cb_event_client_property(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool    _pager_cb_event_client_urgent_change(void *data EINA_UNUSED, int type EINA_UNUSED, E_Event_Client_Property *ev);
static void         _pager_window_move(Pager_Win *pw);
static void         _pager_window_cb_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED);
static void         _pager_window_cb_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info);
static void         _pager_window_cb_mouse_up(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info);
static void         _pager_window_cb_mouse_move(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info);
static void        *_pager_window_cb_drag_convert(E_Drag *drag, const char *type);
static void         _pager_window_cb_drag_finished(E_Drag *drag, int dropped);
static void         _pager_drop_cb_enter(void *data, const char *type EINA_UNUSED, void *event_info);
static void         _pager_drop_cb_move(void *data, const char *type EINA_UNUSED, void *event_info);
static void         _pager_drop_cb_leave(void *data, const char *type EINA_UNUSED, void *event_info EINA_UNUSED);
static void         _pager_drop_cb_drop(void *data, const char *type, void *event_info);
static void         _pager_update_drop_position(Pager *p, Pager_Desk *pd, Evas_Coord x, Evas_Coord y);
static void         _pager_desk_cb_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info);
static void         _pager_desk_cb_mouse_up(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info);
static void         _pager_desk_cb_mouse_move(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info);
static void         _pager_desk_cb_drag_finished(E_Drag *drag, int dropped);
static void         _pager_desk_cb_mouse_wheel(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info);
static Eina_Bool    _pager_popup_cb_timeout(void *data);
static Pager       *_pager_new(Evas *evas, Eina_Bool popup);
static void         _pager_free(Pager *p);
static void         _pager_fill(Pager *p);
static void         _pager_orient(Instance *inst, E_Gadget_Site_Orient orient);
static void         _pager_empty(Pager *p);
static Pager_Desk  *_pager_desk_new(Pager *p, E_Desk *desk, int xpos, int ypos, Eina_Bool invert);
static void         _pager_desk_free(Pager_Desk *pd);
static void         _pager_desk_select(Pager_Desk *pd);
static Pager_Desk  *_pager_desk_find(Pager *p, E_Desk *desk);
static void         _pager_desk_switch(Pager_Desk *pd1, Pager_Desk *pd2);
static Pager_Win   *_pager_window_new(Pager_Desk *pd, Evas_Object *mirror, E_Client *client);
static void         _pager_window_free(Pager_Win *pw);
static Pager_Popup *pager_popup_new(int keyaction);
static void         _pager_popup_free(Pager_Popup *pp);
static Pager_Popup *_pager_popup_find(E_Zone *zone);

/* functions for pager popup on key actions */
static int          _pager_popup_show(void);
static void         _pager_popup_hide(int switch_desk);
static Eina_Bool    _pager_popup_cb_mouse_wheel(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static void         _pager_popup_desk_switch(int x, int y);
static void         _pager_popup_modifiers_set(int mod);
static Eina_Bool    _pager_popup_cb_key_down(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool    _pager_popup_cb_key_up(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static void         _pager_popup_cb_action_show(E_Object *obj EINA_UNUSED, const char *params EINA_UNUSED, Ecore_Event_Key *ev EINA_UNUSED);
static void         _pager_popup_cb_action_switch(E_Object *obj EINA_UNUSED, const char *params, Ecore_Event_Key *ev);

/* variables for pager popup on key actions */
static Ecore_Window input_window = 0;
static Eina_List *handlers = NULL;
static Pager_Popup *act_popup = NULL; /* active popup */
static int hold_count = 0;
static int hold_mod = 0;
static E_Desk *current_desk = NULL;
static Eina_List *pagers = NULL;

static E_Action *act_popup_show = NULL;
static E_Action *act_popup_switch = NULL;

static const char *
_pager_location_get(Instance *inst)
{
   const char *s = "float";

   if (!inst) return NULL;

   E_Gadget_Site_Orient orient = e_gadget_site_orient_get(e_gadget_site_get(inst->o_pager));
   E_Gadget_Site_Anchor anchor = e_gadget_site_anchor_get(e_gadget_site_get(inst->o_pager));

   if (anchor & E_GADGET_SITE_ANCHOR_LEFT)
     {
        if (anchor & E_GADGET_SITE_ANCHOR_TOP)
          {
             switch (orient)
               {
                case E_GADGET_SITE_ORIENT_HORIZONTAL:
                  s = "top";
                  break;

                case E_GADGET_SITE_ORIENT_VERTICAL:
                  s = "left";
                  break;

                case E_GADGET_SITE_ORIENT_NONE:
                  s = "left";
                  break;
               }
          }
        else if (anchor & E_GADGET_SITE_ANCHOR_BOTTOM)
          {
             switch (orient)
               {
                case E_GADGET_SITE_ORIENT_HORIZONTAL:
                  s = "bottom";
                  break;

                case E_GADGET_SITE_ORIENT_VERTICAL:
                  s = "left";
                  break;

                case E_GADGET_SITE_ORIENT_NONE:
                  s = "left";
                  break;
               }
          }
        else
          s = "left";
     }
   else if (anchor & E_GADGET_SITE_ANCHOR_RIGHT)
     {
        if (anchor & E_GADGET_SITE_ANCHOR_TOP)
          {
             switch (orient)
               {
                case E_GADGET_SITE_ORIENT_HORIZONTAL:
                  s = "top";
                  break;

                case E_GADGET_SITE_ORIENT_VERTICAL:
                  s = "right";
                  break;

                case E_GADGET_SITE_ORIENT_NONE:
                  s = "right";
                  break;
               }
          }
        else if (anchor & E_GADGET_SITE_ANCHOR_BOTTOM)
          {
             switch (orient)
               {
                case E_GADGET_SITE_ORIENT_HORIZONTAL:
                  s = "bottom";
                  break;

                case E_GADGET_SITE_ORIENT_VERTICAL:
                  s = "right";
                  break;

                case E_GADGET_SITE_ORIENT_NONE:
                  s = "right";
                  break;
               }
          }
        else
          s = "right";
     }
   else if (anchor & E_GADGET_SITE_ANCHOR_TOP)
     s = "top";
   else if (anchor & E_GADGET_SITE_ANCHOR_BOTTOM)
     s = "bottom";
   else
     {
        switch (orient)
          {
           case E_GADGET_SITE_ORIENT_HORIZONTAL:
             s = "bottom";
             break;

           case E_GADGET_SITE_ORIENT_VERTICAL:
             s = "left";
             break;

           default:
             s = "bottom";
             break;
          }
     }
   return s;
}

static Eina_Bool
_pager_check_modifiers(Evas_Modifier *modifiers)
{
   if ((evas_key_modifier_is_set(modifiers, "Alt")) ||
       (evas_key_modifier_is_set(modifiers, "Control")) ||
       (evas_key_modifier_is_set(modifiers, "Shift")))
     return EINA_TRUE;
   return EINA_FALSE;
}

static Pager_Win *
_pager_desk_window_find(Pager_Desk *pd, E_Client *client)
{
   Eina_List *l;
   Pager_Win *pw;

   EINA_LIST_FOREACH(pd->wins, l, pw)
     {
        if (pw)
          {
             if (pw->client == client) return pw;
          }
     }
   return NULL;
}

static Pager_Win *
_pager_window_find(Pager *p, E_Client *client)
{
   Eina_List *l;
   Pager_Desk *pd;

   EINA_LIST_FOREACH(p->desks, l, pd)
     {
        Pager_Win *pw;

        pw = _pager_desk_window_find(pd, client);
        if (pw) return pw;
     }
   return NULL;
}

static void
_pager_gadget_anchor_change_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   Eina_Bool invert;

   if (inst->pager && inst->o_pager)
     {
        switch (e_gadget_site_orient_get(obj))
          {
           case E_GADGET_SITE_ORIENT_HORIZONTAL:
             invert = EINA_FALSE;
             break;

           case E_GADGET_SITE_ORIENT_VERTICAL:
             invert = EINA_TRUE;
             break;

           default:
             invert = EINA_FALSE;
          }
        if (invert != inst->pager->invert)
          {
             _pager_orient(inst, e_gadget_site_orient_get(obj));
             _pager_empty(inst->pager);
             _pager_fill(inst->pager);
          }
     }
}

static void
_pager_gadget_created_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   Eina_List *l;
   Pager_Desk *pd;
   if (inst->pager && inst->o_pager)
     {
        e_gadget_configure_cb_set(inst->o_pager, _pager_gadget_configure);
        _pager_orient(inst, e_gadget_site_orient_get(obj));
        _pager_empty(inst->pager);
        _pager_fill(inst->pager);

        EINA_LIST_FOREACH(inst->pager->desks, l, pd)
          {
             if (!pd->drop_handler)
               {
                  const char *drop[] =
                  {
                     "enlightenment/pager_win", "enlightenment/border",
                     "enlightenment/vdesktop"
                  };
                  pd->drop_handler =
                    e_gadget_drop_handler_add(inst->o_pager, pd,
                                              _pager_drop_cb_enter, _pager_drop_cb_move,
                                              _pager_drop_cb_leave, _pager_drop_cb_drop,
                                              drop, 3);
                  //edje_object_part_swallow(pd->o_desk, "e.swallow.drop", pd->drop_handler);
                  evas_object_show(pd->drop_handler);
               }
          }
     }
   evas_object_smart_callback_del_full(obj, "gadget_created", _pager_gadget_created_cb, data);
}

static void
_pager_orient(Instance *inst, E_Gadget_Site_Orient orient)
{
   int aspect_w, aspect_h, zone_w = 1, zone_h = 1;

   if (inst->o_pager)
     {
        switch (orient)
          {
           case E_GADGET_SITE_ORIENT_HORIZONTAL:
             inst->pager->invert = EINA_FALSE;
             break;

           case E_GADGET_SITE_ORIENT_VERTICAL:
             inst->pager->invert = EINA_TRUE;
             break;

           default:
             inst->pager->invert = EINA_FALSE;
          }
     }
   if (inst->pager->zone)
     {
        zone_w = inst->pager->zone->w;
        zone_h = inst->pager->zone->h;
     }
   if (inst->pager->invert)
     {
        aspect_w = inst->pager->ynum * zone_w;
        aspect_h = inst->pager->xnum * zone_h;
        evas_object_size_hint_aspect_set(inst->o_pager, EVAS_ASPECT_CONTROL_BOTH, aspect_w, aspect_h);
     }
   else
     {
        aspect_w = inst->pager->xnum * zone_w;
        aspect_h = inst->pager->ynum * zone_h;
        evas_object_size_hint_aspect_set(inst->o_pager, EVAS_ASPECT_CONTROL_BOTH, aspect_w, aspect_h);
     }
}

static void
_pager_recalc(void *data)
{
   Pager *p = data;
   Pager_Desk *pd;
   Evas_Coord mw = 0, mh = 0;
   int w, h, zw, zh, w2, h2;

   p->recalc = NULL;
   if (!p->zone) return;
   zw = p->zone->w; zh = p->zone->h;
   pd = eina_list_data_get(p->desks);
   if (!pd) return;

   edje_object_size_min_calc(pd->o_desk, &mw, &mh);
   evas_object_geometry_get(pd->o_desk, NULL, NULL, &w, &h);
   w -= mw; h -= mh;
   w2 = w; h2 = (zh * w) / zw;
   if (h2 > h)
     {
        h2 = h; w2 = (zw * h) / zh;
     }
   w = w2; h = h2;
   w += mw; h += mh;
   if ((p->inst) && (p->inst->o_pager))
     {
        _pager_orient(p->inst, e_gadget_site_orient_get(e_gadget_site_get(p->inst->o_pager)));
     }
}

static void
_pager_resize(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Pager *p = data;

   if (!p->recalc)
     p->recalc = ecore_job_add(_pager_recalc, p);
}

static Pager *
_pager_new(Evas *evas, Eina_Bool popup)
{
   Pager *p;

   p = E_NEW(Pager, 1);
   p->inst = NULL;
   p->popup = NULL;
   if (pager_config->plain)
     {
        switch (e_powersave_mode_get())
          {
           case E_POWERSAVE_MODE_HIGH:
             p->plain = EINA_TRUE;
             break;

           case E_POWERSAVE_MODE_EXTREME:
             p->plain = EINA_TRUE;
             break;

           case E_POWERSAVE_MODE_FREEZE:
             p->plain = EINA_TRUE;
             break;

           default:
             p->plain = EINA_FALSE;
          }
     }
   p->o_table = elm_table_add(e_win_evas_win_get(evas));
   evas_object_event_callback_add(p->o_table, EVAS_CALLBACK_RESIZE, _pager_resize, p);
   elm_table_homogeneous_set(p->o_table, 1);
   if (popup)
     p->zone = e_zone_current_get();
   else
     p->zone = e_comp_object_util_zone_get(p->o_table);
   _pager_fill(p);
   pagers = eina_list_append(pagers, p);
   return p;
}

static void
_pager_free(Pager *p)
{
   _pager_empty(p);
   evas_object_del(p->o_table);
   ecore_job_del(p->recalc);
   pagers = eina_list_remove(pagers, p);
   free(p);
}

static void
_pager_fill(Pager *p)
{
   int x, y;
   E_Gadget_Site_Orient orient;

   if (p->inst && p->inst->o_pager)
     {
        orient = e_gadget_site_orient_get(e_gadget_site_get(p->inst->o_pager));
        switch (orient)
          {
           case E_GADGET_SITE_ORIENT_HORIZONTAL:
             p->invert = EINA_FALSE;
             break;

           case E_GADGET_SITE_ORIENT_VERTICAL:
             p->invert = EINA_TRUE;
             break;

           default:
             p->invert = EINA_FALSE;
          }
     }
   e_zone_desk_count_get(p->zone, &(p->xnum), &(p->ynum));
   if (p->ynum != 1) p->invert = EINA_FALSE;
   for (x = 0; x < p->xnum; x++)
     {
        if ((p->plain) || (pager_config->permanent_plain))
          {
             if (!phandlers)
               {
                  E_LIST_HANDLER_APPEND(phandlers, E_EVENT_CLIENT_RESIZE, _pager_cb_event_client_resize, NULL);
                  E_LIST_HANDLER_APPEND(phandlers, E_EVENT_CLIENT_MOVE, _pager_cb_event_client_move, NULL);
                  E_LIST_HANDLER_APPEND(phandlers, E_EVENT_CLIENT_ADD, _pager_cb_event_client_add, NULL);
                  E_LIST_HANDLER_APPEND(phandlers, E_EVENT_CLIENT_REMOVE, _pager_cb_event_client_remove, NULL);
                  E_LIST_HANDLER_APPEND(phandlers, E_EVENT_CLIENT_ICONIFY, _pager_cb_event_client_iconify, NULL);
                  E_LIST_HANDLER_APPEND(phandlers, E_EVENT_CLIENT_UNICONIFY, _pager_cb_event_client_uniconify, NULL);
                  E_LIST_HANDLER_APPEND(phandlers, E_EVENT_CLIENT_PROPERTY, _pager_cb_event_client_stick, NULL);
                  E_LIST_HANDLER_APPEND(phandlers, E_EVENT_CLIENT_PROPERTY, _pager_cb_event_client_unstick, NULL);
                  E_LIST_HANDLER_APPEND(phandlers, E_EVENT_CLIENT_DESK_SET, _pager_cb_event_client_desk_set, NULL);
                  E_LIST_HANDLER_APPEND(phandlers, E_EVENT_CLIENT_STACK, _pager_cb_event_client_stack, NULL);
                  E_LIST_HANDLER_APPEND(phandlers, E_EVENT_CLIENT_PROPERTY, _pager_cb_event_client_icon_change, NULL);
                  E_LIST_HANDLER_APPEND(phandlers, E_EVENT_CLIENT_FOCUS_IN, _pager_cb_event_client_focus_in, NULL);
                  E_LIST_HANDLER_APPEND(phandlers, E_EVENT_CLIENT_FOCUS_OUT, _pager_cb_event_client_focus_out, NULL);
                  E_LIST_HANDLER_APPEND(phandlers, E_EVENT_CLIENT_PROPERTY, _pager_cb_event_client_property, NULL);
               }
          }
        for (y = 0; y < p->ynum; y++)
          {
             Pager_Desk *pd;
             E_Desk *desk;

             desk = e_desk_at_xy_get(p->zone, x, y);
             if (desk)
               {
                  pd = _pager_desk_new(p, desk, x, y, p->invert);
                  if (pd)
                    {
                       p->desks = eina_list_append(p->desks, pd);
                       if (desk == e_desk_current_get(desk->zone))
                         _pager_desk_select(pd);
                    }
               }
          }
     }
}

static void
_pager_empty(Pager *p)
{
   p->active_pd = NULL;
   if (!(p->plain) && !(pager_config->permanent_plain))
     {
        if (!phandlers)
          {
             Ecore_Event_Handler *handler;
             EINA_LIST_FREE(phandlers, handler)
               ecore_event_handler_del(handler);
          }
     }
   E_FREE_LIST(p->desks, _pager_desk_free);
}

static Pager_Desk *
_pager_desk_new(Pager *p, E_Desk *desk, int xpos, int ypos, Eina_Bool invert)
{
   Pager_Desk *pd;
   Evas_Object *o, *evo;
   E_Client *ec;
   Eina_List *l;
   int w, h;
   Evas *e;
   const char *drop[] =
   {
      "enlightenment/pager_win", "enlightenment/border",
      "enlightenment/vdesktop"
   };

   if (!desk) return NULL;
   pd = E_NEW(Pager_Desk, 1);
   if (!pd) return NULL;

   pd->xpos = xpos;
   pd->ypos = ypos;
   pd->urgent = 0;
   pd->desk = desk;
   e_object_ref(E_OBJECT(desk));
   pd->pager = p;
   pd->drop_handler = NULL;

   e = evas_object_evas_get(p->o_table);
   o = edje_object_add(e);
   pd->o_desk = o;
   if (p->plain || pager_config->permanent_plain)
     e_theme_edje_object_set(o, "base/theme/gadgets/pager",
                             "e/gadget/pager/plain/desk");
   else
     e_theme_edje_object_set(o, "base/theme/gadgets/pager",
                             "e/gadget/pager/default/desk");
   edje_object_part_text_set(o, "e.text.label", desk->name);
   if (pager_config->show_desk_names)
     edje_object_signal_emit(o, "e,name,show", "e");

   edje_object_size_min_calc(o, &w, &h);

   E_EXPAND(o);
   E_FILL(o);
   if (invert)
     elm_table_pack(p->o_table, o, ypos, xpos, 1, 1);
   else
     elm_table_pack(p->o_table, o, xpos, ypos, 1, 1);

   evo = (Evas_Object *)edje_object_part_object_get(o, "e.eventarea");
   if (!evo) evo = o;

   evas_object_event_callback_add(evo, EVAS_CALLBACK_MOUSE_DOWN,
                                  _pager_desk_cb_mouse_down, pd);
   evas_object_event_callback_add(evo, EVAS_CALLBACK_MOUSE_UP,
                                  _pager_desk_cb_mouse_up, pd);
   evas_object_event_callback_add(evo, EVAS_CALLBACK_MOUSE_MOVE,
                                  _pager_desk_cb_mouse_move, pd);
   evas_object_event_callback_add(evo, EVAS_CALLBACK_MOUSE_WHEEL,
                                  _pager_desk_cb_mouse_wheel, pd);
   evas_object_show(o);

   if (p->plain || pager_config->permanent_plain)
     {
        pd->o_layout = e_layout_add(e);
        e_layout_virtual_size_set(pd->o_layout, desk->zone->w, desk->zone->h);
        edje_object_part_swallow(pd->o_desk, "e.swallow.content", pd->o_layout);
        evas_object_show(pd->o_layout);
        E_CLIENT_FOREACH(ec)
        {
           Pager_Win *pw;

           if (e_client_util_ignored_get(ec)) continue;
           if ((ec->new_client) || (ec->zone != desk->zone) ||
               ((ec->desk != desk) && (!ec->sticky)))
             continue;
           pw = _pager_window_new(pd, NULL, ec);
           if (pw) pd->wins = eina_list_append(pd->wins, pw);
        }
     }
   else
     {
        pd->o_layout = e_deskmirror_add(desk, 1, 0);
        evas_object_smart_callback_add(pd->o_layout, "mirror_add", (Evas_Smart_Cb)_pager_cb_mirror_add, pd);

        l = e_deskmirror_mirror_list(pd->o_layout);
        EINA_LIST_FREE(l, o)
          {
             ec = evas_object_data_get(o, "E_Client");
             if (ec)
               {
                  Pager_Win *pw;

                  pw = _pager_window_new(pd, o, ec);
                  if (pw) pd->wins = eina_list_append(pd->wins, pw);
               }
          }
        edje_object_part_swallow(pd->o_desk, "e.swallow.content", pd->o_layout);
        evas_object_show(pd->o_layout);
     }
   if (pd->pager->inst)
     {
        pd->drop_handler =
          e_gadget_drop_handler_add(p->inst->o_pager, pd,
                                    _pager_drop_cb_enter, _pager_drop_cb_move,
                                    _pager_drop_cb_leave, _pager_drop_cb_drop,
                                    drop, 3);
        edje_object_part_swallow(pd->o_desk, "e.swallow.drop", pd->drop_handler);
        evas_object_show(pd->drop_handler);
     }

   return pd;
}

static void
_pager_desk_free(Pager_Desk *pd)
{
   Pager_Win *w;

   if (pd->drop_handler)
     evas_object_del(pd->drop_handler);
   pd->drop_handler = NULL;
   evas_object_del(pd->o_desk);
   evas_object_del(pd->o_layout); 
   EINA_LIST_FREE(pd->wins, w)
       _pager_window_free(w);
   e_object_unref(E_OBJECT(pd->desk));
   free(pd);
}

static void
_pager_desk_select(Pager_Desk *pd)
{
   char ori[32];
   const char *s = _pager_location_get(pd->pager->inst);
   if (pd->current) return;
   if (pd->pager->active_pd)
     {
        pd->pager->active_pd->current = 0;
        edje_object_signal_emit(pd->pager->active_pd->o_desk, "e,state,unselected", "e");
     }
   pd->current = 1;
   evas_object_raise(pd->o_desk);
   if (s)
     snprintf(ori, sizeof(ori), "e,state,selected,%s", s);
   else
     snprintf(ori, sizeof(ori), "e,state,selected,bottom");
   edje_object_signal_emit(pd->o_desk, ori, "e");
   pd->pager->active_pd = pd;
}

static Pager_Desk *
_pager_desk_find(Pager *p, E_Desk *desk)
{
   Eina_List *l;
   Pager_Desk *pd;

   EINA_LIST_FOREACH(p->desks, l, pd)
     if (pd->desk == desk) return pd;

   return NULL;
}

static void
_pager_desk_switch(Pager_Desk *pd1, Pager_Desk *pd2)
{
   int c;
   E_Zone *zone1, *zone2;
   E_Desk *desk1, *desk2;
   Pager_Win *pw;
   Eina_List *l;

   if ((!pd1) || (!pd2) || (!pd1->desk) || (!pd2->desk)) return;
   if (pd1 == pd2) return;

   desk1 = pd1->desk;
   desk2 = pd2->desk;
   zone1 = pd1->desk->zone;
   zone2 = pd2->desk->zone;

   /* Move opened windows from on desk to the other */
   EINA_LIST_FOREACH(pd1->wins, l, pw)
     {
        if ((!pw) || (!pw->client) || (pw->client->iconic)) continue;
        pw->client->hidden = 0;
        e_client_desk_set(pw->client, desk2);
     }
   EINA_LIST_FOREACH(pd2->wins, l, pw)
     {
        if ((!pw) || (!pw->client) || (pw->client->iconic)) continue;
        pw->client->hidden = 0;
        e_client_desk_set(pw->client, desk1);
     }

   /* Modify desktop names in the config */
   for (l = e_config->desktop_names, c = 0; l && c < 2; l = l->next)
     {
        E_Config_Desktop_Name *tmp_dn;

        tmp_dn = l->data;
        if (!tmp_dn) continue;
        if ((tmp_dn->desk_x == desk1->x) &&
            (tmp_dn->desk_y == desk1->y) &&
            (tmp_dn->zone == (int)desk1->zone->num))
          {
             tmp_dn->desk_x = desk2->x;
             tmp_dn->desk_y = desk2->y;
             tmp_dn->zone = desk2->zone->num;
             c++;
          }
        else if ((tmp_dn->desk_x == desk2->x) &&
                 (tmp_dn->desk_y == desk2->y) &&
                 (tmp_dn->zone == (int)desk2->zone->num))
          {
             tmp_dn->desk_x = desk1->x;
             tmp_dn->desk_y = desk1->y;
             tmp_dn->zone = desk1->zone->num;
             c++;
          }
     }
   if (c > 0) e_config_save();
   e_desk_name_update();

   /* Modify desktop backgrounds in the config */
   for (l = e_config->desktop_backgrounds, c = 0; l && c < 2; l = l->next)
     {
        E_Config_Desktop_Background *tmp_db;

        tmp_db = l->data;
        if (!tmp_db) continue;
        if ((tmp_db->desk_x == desk1->x) &&
            (tmp_db->desk_y == desk1->y) &&
            (tmp_db->zone == (int)desk1->zone->num))
          {
             tmp_db->desk_x = desk2->x;
             tmp_db->desk_y = desk2->y;
             tmp_db->zone = desk2->zone->num;
             c++;
          }
        else if ((tmp_db->desk_x == desk2->x) &&
                 (tmp_db->desk_y == desk2->y) &&
                 (tmp_db->zone == (int)desk2->zone->num))
          {
             tmp_db->desk_x = desk1->x;
             tmp_db->desk_y = desk1->y;
             tmp_db->zone = desk1->zone->num;
             c++;
          }
     }
   if (c > 0) e_config_save();

   /* If the current desktop has been switched, force to update of the screen */
   if (desk2 == e_desk_current_get(zone2))
     {
        desk2->visible = 0;
        e_desk_show(desk2);
     }
   if (desk1 == e_desk_current_get(zone1))
     {
        desk1->visible = 0;
        e_desk_show(desk1);
     }
}

static Pager_Win *
_pager_window_new(Pager_Desk *pd, Evas_Object *mirror, E_Client *client)
{
   Pager_Win *pw;
   Evas_Object *o;
   int visible;

   if (!client) return NULL;
   pw = E_NEW(Pager_Win, 1);
   if (!pw) return NULL;

   pw->client = client;
   pw->desk = pd;
   if ((pd->pager->plain) || (pager_config->permanent_plain))
     {
        visible = ((!client->iconic) && (!client->netwm.state.skip_pager));
        pw->skip_winlist = client->netwm.state.skip_pager;

        o = edje_object_add(evas_object_evas_get(pd->pager->o_table));
        pw->o_window = o;
        e_theme_edje_object_set(o, "base/theme/gadgets/pager",
                                "e/gadget/pager/plain/window");
        if (visible) evas_object_show(o);
        e_layout_pack(pd->o_layout, pw->o_window);
        e_layout_child_raise(pw->o_window);
        o = e_client_icon_add(client, evas_object_evas_get(pd->pager->o_table));
        if (o)
          {
             pw->o_icon = o;
             evas_object_show(o);
             edje_object_part_swallow(pw->o_window, "e.swallow.icon", o);
          }
        e_layout_child_move(pw->o_window,
                            pw->client->x - pw->client->zone->x,
                            pw->client->y - pw->client->zone->y);
        e_layout_child_resize(pw->o_window, pw->client->w, pw->client->h);
        evas_object_show(o);
        o = pw->o_window;
     }
   else
     {
        pw->o_mirror = mirror;
        o = mirror;
     }
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                  _pager_window_cb_mouse_down, pw);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_UP,
                                  _pager_window_cb_mouse_up, pw);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_MOVE,
                                  _pager_window_cb_mouse_move, pw);
   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL,
                                  _pager_window_cb_del, pw);

   if (client->urgent)
     {
        if (!(client->iconic))
          {
             if ((pd->pager->plain) || (pager_config->permanent_plain))
               edje_object_signal_emit(pw->o_window, "e,state,urgent", "e");
             else
               edje_object_signal_emit(pd->o_desk, "e,state,urgent", "e");
          }
     }

   return pw;
}

static void
_pager_window_free(Pager_Win *pw)
{
   Evas_Object *o;
   if ((pw->drag.from_pager) && (pw->desk->pager->dragging))
     pw->desk->pager->dragging = 0;
   if (pw->o_mirror)
     o = pw->o_mirror;
   else
     o = pw->o_window;
   if (o)
     evas_object_event_callback_del_full(o, EVAS_CALLBACK_DEL,
                                         _pager_window_cb_del, pw);
   if (pw->o_icon) evas_object_del(pw->o_icon);
   if (pw->o_window) evas_object_del(pw->o_window);
   free(pw);
}

static void
_pager_popup_cb_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Pager_Popup *pp = data;
   E_FREE_FUNC(pp->timer, ecore_timer_del);
   _pager_free(pp->pager);
   free(pp);
}

static Pager_Popup *
pager_popup_new(int keyaction)
{
   Pager_Popup *pp;
   Evas_Coord w, h, zx, zy, zw, zh;
   int x, y, height, width;
   E_Desk *desk;
   Pager_Desk *pd;
   E_Zone *zone = e_zone_current_get();

   pp = E_NEW(Pager_Popup, 1);
   if (!pp) return NULL;

   /* Show popup */

   pp->pager = _pager_new(e_comp->evas, EINA_TRUE);

   pp->pager->popup = pp;
   pp->urgent = 0;

   e_zone_desk_count_get(zone, &x, &y);

   if (keyaction)
     height = pager_config->popup_act_height;
   else
     height = pager_config->popup_height;

   pd = eina_list_data_get(pp->pager->desks);
   if (!pd)
     {
        height *= y;
        width = height * (zone->w * x) / (zone->h * y);
     }
   else
     {
        Evas_Coord mw = 0, mh = 0;

        edje_object_size_min_calc(pd->o_desk, &mw, &mh);
        height -= mh;
        width = (height * zone->w) / zone->h;
        height *= y;
        height += (y * mh);
        width *= x;
        width += (x * mw);
     }

   evas_object_move(pp->pager->o_table, 0, 0);
   evas_object_resize(pp->pager->o_table, width, height);

   pp->o_bg = edje_object_add(e_comp->evas);
   evas_object_name_set(pp->o_bg, "pager_gadget_popup");
   e_theme_edje_object_set(pp->o_bg, "base/theme/gadgets/pager",
                           "e/gadget/pager/popup");
   desk = e_desk_current_get(zone);
   if (desk)
     edje_object_part_text_set(pp->o_bg, "e.text.label", desk->name);

   evas_object_size_hint_min_set(pp->pager->o_table, width, height);
   edje_object_part_swallow(pp->o_bg, "e.swallow.content", pp->pager->o_table);
   edje_object_size_min_calc(pp->o_bg, &w, &h);

   pp->popup = e_comp_object_util_add(pp->o_bg, E_COMP_OBJECT_TYPE_POPUP);
   evas_object_layer_set(pp->popup, E_LAYER_CLIENT_POPUP);
   evas_object_pass_events_set(pp->popup, 1);
   e_zone_useful_geometry_get(zone, &zx, &zy, &zw, &zh);
   evas_object_geometry_set(pp->popup, zx, zy, w, h);
   e_comp_object_util_center(pp->popup);
   evas_object_event_callback_add(pp->popup, EVAS_CALLBACK_DEL, _pager_popup_cb_del, pp);
   evas_object_show(pp->popup);

   pp->timer = NULL;

   return pp;
}

static void
_pager_popup_free(Pager_Popup *pp)
{
   E_FREE_FUNC(pp->timer, ecore_timer_del);
   evas_object_hide(pp->popup);
   evas_object_del(pp->popup);
}

static Pager_Popup *
_pager_popup_find(E_Zone *zone)
{
   Eina_List *l;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     if ((p->popup) && (p->zone == zone))
       return p->popup;

   return NULL;
}

static void
_pager_cb_obj_hide(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   Eina_List *l;
   Pager_Desk *pd;

   EINA_LIST_FOREACH(inst->pager->desks, l, pd)
     edje_object_signal_emit(pd->o_desk, "e,state,hidden", "e");
}

static void
_pager_cb_obj_show(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   Eina_List *l;
   Pager_Desk *pd;

   EINA_LIST_FOREACH(inst->pager->desks, l, pd)
     edje_object_signal_emit(pd->o_desk, "e,state,visible", "e");
}

static void
_pager_cb_move(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;
   Pager *p = inst->pager;
   E_Zone *zone = e_comp_object_util_zone_get(p->o_table);
   if (zone != p->zone)
     {
        p->zone = zone;
        _pager_empty(p);
        _pager_fill(p);
     }
}

static Evas_Object *
_pager_gadget_configure(Evas_Object *g EINA_UNUSED)
{
   if (!pager_config) return NULL;
   if (cfg_dialog) return NULL;
   return config_pager(e_zone_current_get());
}

static void
_button_cb_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Instance *inst = data;
   Evas_Event_Mouse_Down *ev = event_info;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (_pager_check_modifiers(ev->modifiers)) return;
   if (ev->button != 3) return;
   if (!pager_config) return;
   if (cfg_dialog) return;
   ev->event_flags |= EVAS_EVENT_FLAG_ON_HOLD;
   e_gadget_configure(inst->o_pager);
}

EINTERN void
_pager_cb_config_gadget_updated(Eina_Bool style_changed)
{
   Pager *p;
   Pager_Desk *pd;
   Eina_List *l, *ll;
   if (!pager_config) return;
   EINA_LIST_FOREACH(pagers, l, p)
     {
        if (style_changed)
          {
             if (pager_config->plain)
               {
                  switch (e_powersave_mode_get())
                    {
                     case E_POWERSAVE_MODE_HIGH:
                       p->plain = EINA_TRUE;
                       break;

                     case E_POWERSAVE_MODE_EXTREME:
                       p->plain = EINA_TRUE;
                       break;

                     case E_POWERSAVE_MODE_FREEZE:
                       p->plain = EINA_TRUE;
                       break;

                     default:
                       p->plain = EINA_FALSE;
                    }
               }
             _pager_empty(p);
             _pager_fill(p);
          }
        EINA_LIST_FOREACH(p->desks, ll, pd)
          {
             if (pd->current)
               {
                  char ori[32];
                  const char *s = _pager_location_get(pd->pager->inst);
                  if (s)
                    snprintf(ori, sizeof(ori), "e,state,selected,%s", s);
                  else
                    snprintf(ori, sizeof(ori), "e,state,selected,bottom");
                  edje_object_signal_emit(pd->o_desk, ori, "e");
               }
             else
               edje_object_signal_emit(pd->o_desk, "e,state,unselected", "e");
             if (pager_config->show_desk_names)
               edje_object_signal_emit(pd->o_desk, "e,name,show", "e");
             else
               edje_object_signal_emit(pd->o_desk, "e,name,hide", "e");
          }
     }
}

static void
_pager_cb_mirror_add(Pager_Desk *pd, Evas_Object *obj EINA_UNUSED, Evas_Object *mirror)
{
   Pager_Win *pw;

   pw = _pager_window_new(pd, mirror, evas_object_data_get(mirror, "E_Client"));
   if (pw) pd->wins = eina_list_append(pd->wins, pw);
}

static Eina_Bool
_pager_cb_event_client_resize(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev = event;
   Eina_List *l, *l2;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     {
        Pager_Desk *pd;

        if (p->zone != ev->ec->zone) continue;
        EINA_LIST_FOREACH(p->desks, l2, pd)
          {
             Pager_Win *pw;

             pw = _pager_desk_window_find(pd, ev->ec);
             if (pw) _pager_window_move(pw);
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_move(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev = event;
   Eina_List *l;
   Pager_Win *pw;
   Pager_Desk *pd;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     {
        Eina_List *l2;

        if (p->zone != ev->ec->zone) continue;
        EINA_LIST_FOREACH(p->desks, l2, pd)
          {
             pw = _pager_desk_window_find(pd, ev->ec);
             if (pw) _pager_window_move(pw);
          }
     }

   if ((act_popup) && (act_popup->pager->zone == ev->ec->zone))
     {
        EINA_LIST_FOREACH(act_popup->pager->desks, l, pd)
          {
             pw = _pager_desk_window_find(pd, ev->ec);
             if (pw) _pager_window_move(pw);
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_add(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev = event;
   Eina_List *l;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     {
        Pager_Desk *pd;
        Pager_Win *pw;

        if ((p->zone != ev->ec->zone) ||
            (_pager_window_find(p, ev->ec)))
          continue;
        pd = _pager_desk_find(p, ev->ec->desk);
        if (!pd) continue;
        pw = _pager_window_new(pd, NULL, ev->ec);
        if (pw) pd->wins = eina_list_append(pd->wins, pw);
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_remove(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev = event;
   Eina_List *l;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     {
        Eina_List *l2;
        Pager_Desk *pd;

        if (p->zone != ev->ec->zone) continue;

        EINA_LIST_FOREACH(p->desks, l2, pd)
          {
             Pager_Win *pw;

             pw = _pager_desk_window_find(pd, ev->ec);
             if (!pw) continue;
             pd->wins = eina_list_remove(pd->wins, pw);
             _pager_window_free(pw);
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_iconify(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev = event;
   Eina_List *l;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     {
        Eina_List *l2;
        Pager_Desk *pd;

        if (p->zone != ev->ec->zone) continue;

        EINA_LIST_FOREACH(p->desks, l2, pd)
          {
             Pager_Win *pw;

             pw = _pager_desk_window_find(pd, ev->ec);
             if (!pw) continue;
             if ((pw->drag.from_pager) && (pw->desk->pager->dragging))
               pw->desk->pager->dragging = 0;
             evas_object_hide(pw->o_window);
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_uniconify(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev = event;
   Eina_List *l;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     {
        Eina_List *l2;
        Pager_Desk *pd;

        if (p->zone != ev->ec->zone) continue;

        EINA_LIST_FOREACH(p->desks, l2, pd)
          {
             Pager_Win *pw;

             pw = _pager_desk_window_find(pd, ev->ec);
             if ((pw) && (!pw->skip_winlist)) evas_object_show(pw->o_window);
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_stick(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client_Property *ev = event;
   Eina_List *l;
   Pager *p;

   if ((!(ev->property & E_CLIENT_PROPERTY_STICKY)) || (!ev->ec->sticky)) return ECORE_CALLBACK_RENEW;
   EINA_LIST_FOREACH(pagers, l, p)
     {
        Eina_List *l2;
        Pager_Desk *pd;
        Pager_Win *pw;

        if (p->zone != ev->ec->zone) continue;
        pw = _pager_window_find(p, ev->ec);
        if (!pw) continue;

        EINA_LIST_FOREACH(p->desks, l2, pd)
          if ((ev->ec->desk != pd->desk) && (!_pager_desk_window_find(pd, ev->ec)))
            {
               pw = _pager_window_new(pd, NULL, ev->ec);
               if (pw) pd->wins = eina_list_append(pd->wins, pw);
            }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_unstick(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client_Property *ev = event;
   Eina_List *l;
   Pager *p;

   if ((!(ev->property & E_CLIENT_PROPERTY_STICKY)) || (ev->ec->sticky)) return ECORE_CALLBACK_RENEW;
   EINA_LIST_FOREACH(pagers, l, p)
     {
        Pager_Desk *pd;
        Eina_List *l2;

        if (p->zone != ev->ec->zone) continue;

        EINA_LIST_FOREACH(p->desks, l2, pd)
          if (ev->ec->desk != pd->desk)
            {
               Pager_Win *pw;

               pw = _pager_desk_window_find(pd, ev->ec);
               if (pw)
                 {
                    pd->wins = eina_list_remove(pd->wins, pw);
                    _pager_window_free(pw);
                 }
            }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static void
_pager_window_desk_change(Pager *pager, E_Client *ec)
{
   Eina_List *l;
   Pager_Win *pw;
   Pager_Desk *pd;

   /* if this pager is not for the zone of the client */
   if (pager->zone != ec->zone)
     {
        /* look at all desks in the pager */
        EINA_LIST_FOREACH(pager->desks, l, pd)
          {
             /* find this client in this desk */
             pw = _pager_desk_window_find(pd, ec);
             if (!pw) continue;
             /* if it is found - remove it. it does not belong in this
              * pager as it probably moves zones */
             pd->wins = eina_list_remove(pd->wins, pw);
             _pager_window_free(pw);
          }
        return;
     }
   /* and this pager zone is for this client */
   /* see if the window is in this pager at all */
   pw = _pager_window_find(pager, ec);
   if (pw)    /* is it sticky */
     {
        if (ec->sticky)
          {
             /* if its sticky and in this pager - its already everywhere, so abort
              * doing anything else */
             return;
          }
        /* move it to the right desk */
        /* find the pager desk of the target desk */
        pd = _pager_desk_find(pager, ec->desk);
        if (pd)
          {
             Pager_Win *pw2 = NULL;
             E_Client *ec_above;

             /* remove it from whatever desk it was on */
             pw->desk->wins = eina_list_remove(pw->desk->wins, pw);
             e_layout_unpack(pw->o_window);

             /* add it to the one its MEANT to be on */
             pw->desk = pd;
             pd->wins = eina_list_append(pd->wins, pw);
             e_layout_pack(pd->o_layout, pw->o_window);

             ec_above = e_util_desk_client_above(pw->client);
             if (ec_above)
               pw2 = _pager_desk_window_find(pd, ec_above);
             if (pw2)
               e_layout_child_lower_below(pw->o_window, pw2->o_window);
             else
               e_layout_child_raise(pw->o_window);

             _pager_window_move(pw);
          }
     }
   /* the client isn't in this pager at all - it must have moved zones */
   else
     {
        if (!ec->sticky)
          {
             /* find the pager desk it needs to go to */
             pd = _pager_desk_find(pager, ec->desk);
             if ((pd) && (!_pager_desk_window_find(pd, ec)))
               {
                  /* create it and add it */
                  pw = _pager_window_new(pd, NULL, ec);
                  if (pw)
                    {
                       Pager_Win *pw2 = NULL;
                       E_Client *ec_above;

                       pd->wins = eina_list_append(pd->wins, pw);
                       ec_above = e_util_desk_client_above(pw->client);
                       if (ec_above)
                         pw2 = _pager_desk_window_find(pd, ec_above);
                       if (pw2)
                         e_layout_child_lower_below(pw->o_window, pw2->o_window);
                       else
                         e_layout_child_raise(pw->o_window);
                       _pager_window_move(pw);
                    }
               }
          }
        else
          {
             /* go through all desks */
             EINA_LIST_FOREACH(pager->desks, l, pd)
               {
                  /* create it and add it */
                  if (_pager_desk_window_find(pd, ec)) continue;
                  pw = _pager_window_new(pd, NULL, ec);
                  if (pw)
                    {
                       Pager_Win *pw2 = NULL;
                       E_Client *ec_above;

                       pd->wins = eina_list_append(pd->wins, pw);
                       ec_above = e_util_desk_client_above(pw->client);
                       if (ec_above)
                         pw2 = _pager_desk_window_find(pd, ec_above);
                       if (pw2)
                         e_layout_child_lower_below(pw->o_window, pw2->o_window);
                       else
                         e_layout_child_raise(pw->o_window);
                       _pager_window_move(pw);
                    }
               }
          }
     }
}

static Eina_Bool
_pager_cb_event_client_desk_set(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client_Desk_Set *ev = event;
   Eina_List *l;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     _pager_window_desk_change(p, ev->ec);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_stack(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev = event;
   Eina_List *l;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     {
        Eina_List *l2;
        Pager_Desk *pd;

        if (p->zone != ev->ec->zone) continue;
        EINA_LIST_FOREACH(p->desks, l2, pd)
          {
             Pager_Win *pw, *pw2 = NULL;

             pw = _pager_desk_window_find(pd, ev->ec);
             if (pw)
               {
                  E_Client *ec;

                  ec = e_util_desk_client_below(ev->ec);
                  if (ec) pw2 = _pager_desk_window_find(pd, ec);
                  if (pw2)
                    {
                       e_layout_child_raise_above(pw->o_window, pw2->o_window);
                       continue;
                    }
                  ec = e_util_desk_client_above(ev->ec);
                  if (ec) pw2 = _pager_desk_window_find(pd, ec);
                  if (pw2)
                    {
                       e_layout_child_lower_below(pw->o_window, pw2->o_window);
                       continue;
                    }
               }
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_icon_change(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client_Property *ev = event;
   Eina_List *l;
   Pager *p;

   if (!(ev->property & E_CLIENT_PROPERTY_ICON)) return ECORE_CALLBACK_RENEW;
   EINA_LIST_FOREACH(pagers, l, p)
     {
        Eina_List *l2;
        Pager_Desk *pd;

        if (p->zone != ev->ec->zone) continue;
        EINA_LIST_FOREACH(p->desks, l2, pd)
          {
             Pager_Win *pw;

             pw = _pager_desk_window_find(pd, ev->ec);
             if (pw)
               {
                  Evas_Object *o;

                  if (pw->o_icon)
                    {
                       evas_object_del(pw->o_icon);
                       pw->o_icon = NULL;
                    }
                  o = e_client_icon_add(ev->ec,
                                        evas_object_evas_get(p->o_table));
                  if (o)
                    {
                       pw->o_icon = o;
                       evas_object_show(o);
                       edje_object_part_swallow(pw->o_window,
                                                "e.swallow.icon", o);
                    }
               }
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_focus_in(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev = event;
   Pager *pager;
   Eina_List *l, *l2;
   Pager_Popup *pp;
   Pager_Desk *pd;
   Pager_Win *pw;
   E_Zone *zone;

   zone = ev->ec->zone;

   EINA_LIST_FOREACH(pagers, l, pager)
     {
        if (pager->zone != zone) continue;

        EINA_LIST_FOREACH(pager->desks, l2, pd)
          {
             pw = _pager_desk_window_find(pd, ev->ec);
             if (pw)
               {
                  edje_object_signal_emit(pw->o_window,
                                          "e,state,focused", "e");
                  break;
               }
          }
     }

   pp = _pager_popup_find(zone);
   if (!pp) return ECORE_CALLBACK_PASS_ON;

   EINA_LIST_FOREACH(pp->pager->desks, l, pd)
     {
        pw = _pager_desk_window_find(pd, ev->ec);
        if (pw)
          {
             edje_object_signal_emit(pw->o_window,
                                     "e,state,focused", "e");
             break;
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_focus_out(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev = event;
   Pager *pager;
   Pager_Popup *pp;
   Pager_Desk *pd;
   Pager_Win *pw;
   E_Zone *zone;
   Eina_List *l;

   zone = ev->ec->zone;

   EINA_LIST_FOREACH(pagers, l, pager)
     {
        Eina_List *l2;

        if (pager->zone != zone) continue;

        EINA_LIST_FOREACH(pager->desks, l2, pd)
          {
             pw = _pager_desk_window_find(pd, ev->ec);
             if (pw)
               {
                  edje_object_signal_emit(pw->o_window,
                                          "e,state,unfocused", "e");
                  break;
               }
          }
     }

   pp = _pager_popup_find(zone);
   if (!pp) return ECORE_CALLBACK_PASS_ON;

   EINA_LIST_FOREACH(pp->pager->desks, l, pd)
     {
        pw = _pager_desk_window_find(pd, ev->ec);
        if (pw)
          {
             edje_object_signal_emit(pw->o_window,
                                     "e,state,unfocused", "e");
             break;
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_property(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client_Property *ev = event;
   Eina_List *l, *l2;
   int found = 0;
   Pager *p;
   Pager_Win *pw;
   Pager_Desk *pd;

   if (!(ev->property & E_CLIENT_PROPERTY_NETWM_STATE)) return ECORE_CALLBACK_RENEW;
   EINA_LIST_FOREACH(pagers, l, p)
     {
        if (p->zone != ev->ec->zone) continue;

        EINA_LIST_FOREACH(p->desks, l2, pd)
          {
             pw = _pager_desk_window_find(pd, ev->ec);
             if (pw)
               {
                  found = 1;
                  if (ev->ec->netwm.state.skip_pager)
                    {
                       pd->wins = eina_list_remove(pd->wins, pw);
                       _pager_window_free(pw);
                    }
               }
          }
     }
   if (found) return ECORE_CALLBACK_PASS_ON;

   /* If we did not find this window in the pager, then add it because
    * the skip_pager state may have changed to 1 */
   EINA_LIST_FOREACH(pagers, l, p)
     {
        if ((p->zone != ev->ec->zone) ||
            (_pager_window_find(p, ev->ec)))
          continue;
        if (!ev->ec->sticky)
          {
             pd = _pager_desk_find(p, ev->ec->desk);
             if ((pd) && (!_pager_desk_window_find(pd, ev->ec)))
               {
                  pw = _pager_window_new(pd, NULL, ev->ec);
                  if (pw)
                    {
                       Pager_Win *pw2 = NULL;
                       E_Client *ec;

                       pd->wins = eina_list_append(pd->wins, pw);
                       ec = e_util_desk_client_above(pw->client);
                       if (ec)
                         pw2 = _pager_desk_window_find(pd, ec);
                       if (pw2)
                         e_layout_child_lower_below(pw->o_window, pw2->o_window);
                       else
                         e_layout_child_raise(pw->o_window);
                       _pager_window_move(pw);
                    }
               }
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_zone_desk_count_set(void *data EINA_UNUSED, int type EINA_UNUSED, E_Event_Zone_Desk_Count_Set *ev)
{
   E_Desk *desk;
   Eina_List *l, *ll;
   Pager *p;
   Pager_Desk *pd = NULL;
   int x, y, xx, yy;

   xx = ev->zone->desk_x_count;
   yy = ev->zone->desk_y_count;
   EINA_LIST_FOREACH(pagers, l, p)
     {
        if ((xx == p->xnum) && (yy == p->ynum))
          continue;
        for (x = 0; x < xx; x++)
          {
             for (y = 0; y < yy; y++)
               {
                  if ((x >= p->xnum) || (y >= p->ynum))
                    {
                       desk = e_desk_at_xy_get(p->zone, x, y);
                       if (desk)
                         pd = _pager_desk_new(p, desk, x, y, p->invert);
                       if (pd)
                         p->desks = eina_list_append(p->desks, pd);
                    }
               }
          }
        EINA_LIST_FOREACH(p->desks, ll, pd)
          {
             if (!e_desk_at_xy_get(p->zone, pd->xpos, pd->ypos))
               {
                  p->desks = eina_list_remove(p->desks, pd);
                  _pager_desk_free(pd);
               }
          }
        e_zone_desk_count_get(p->zone, &(p->xnum), &(p->ynum));
        if (p->inst) _pager_orient(p->inst, e_gadget_site_orient_get(e_gadget_site_get(p->inst->o_pager)));
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_desk_show(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Desk_Show *ev = event;
   Eina_List *l;
   Pager *p;
   Pager_Popup *pp;
   Pager_Desk *pd;

   if (!pagers)
     return ECORE_CALLBACK_PASS_ON;

   EINA_LIST_FOREACH(pagers, l, p)
     {
        if (p->zone != ev->desk->zone) continue;
        pd = _pager_desk_find(p, ev->desk);
        if (pd) _pager_desk_select(pd);

        if (p->popup)
          edje_object_part_text_set(p->popup->o_bg, "e.text.label", ev->desk->name);
     }

   if ((pager_config->popup) && (!act_popup))
     {
        if ((pp = _pager_popup_find(ev->desk->zone)))
          evas_object_show(pp->popup);
        else
          pp = pager_popup_new(0);
        if (pp->timer)
          ecore_timer_loop_reset(pp->timer);
        else
          pp->timer = ecore_timer_loop_add(pager_config->popup_speed,
                                           _pager_popup_cb_timeout, pp);
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_desk_name_change(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Desk_Name_Change *ev = event;
   Eina_List *l;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     {
        Pager_Desk *pd;

        if (p->zone != ev->desk->zone) continue;
        pd = _pager_desk_find(p, ev->desk);
        if (pager_config->show_desk_names)
          {
             if (pd)
               edje_object_part_text_set(pd->o_desk, "e.text.label",
                                         ev->desk->name);
          }
        else
          {
             if (pd)
               edje_object_part_text_set(pd->o_desk, "e.text.label", "");
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_client_urgent_change(void *data EINA_UNUSED, int type EINA_UNUSED, E_Event_Client_Property *ev)
{
   Eina_List *l, *l2;
   Pager *p;
   Pager_Desk *pd;
   Pager_Win *pw;

   if (!(ev->property & E_CLIENT_PROPERTY_URGENCY)) return ECORE_CALLBACK_RENEW;
   if (!pagers) return ECORE_CALLBACK_RENEW;

   if (pager_config->popup_urgent && (!e_client_util_desk_visible(ev->ec, e_desk_current_get(ev->ec->zone))) &&
       (pager_config->popup_urgent_focus ||
        (!pager_config->popup_urgent_focus && (!ev->ec->focused) && (!ev->ec->want_focus))))
     {
        Pager_Popup *pp;

        pp = _pager_popup_find(ev->ec->zone);

        if ((!pp) && (ev->ec->urgent || ev->ec->icccm.urgent) && (!ev->ec->iconic))
          {
             pp = pager_popup_new(0);
             if (!pp) return ECORE_CALLBACK_RENEW;

             if (!pager_config->popup_urgent_stick)
               pp->timer = ecore_timer_loop_add(pager_config->popup_urgent_speed,
                                                _pager_popup_cb_timeout, pp);
             pp->urgent = 1;
          }
     }
   EINA_LIST_FOREACH(pagers, l, p)
     {
        if (p->zone != ev->ec->zone) continue;

        EINA_LIST_FOREACH(p->desks, l2, pd)
          {
             pw = _pager_desk_window_find(pd, ev->ec);
             if (pw)
               {
                  if (ev->ec->urgent)
                    {
                       if (!(ev->ec->iconic))
                         {
                            if ((pd->pager) && (pd->pager->inst) &&
                                (!pager_config->popup_urgent))
                              edje_object_signal_emit(pd->o_desk,
                                                      "e,state,urgent", "e");
                         }
                       edje_object_signal_emit(pw->o_window,
                                               "e,state,urgent", "e");
                    }
                  else
                    {
                       if (!(ev->ec->iconic))
                         edje_object_signal_emit(pd->o_desk,
                                                 "e,state,not_urgent", "e");
                       edje_object_signal_emit(pw->o_window,
                                               "e,state,not_urgent", "e");
                    }
               }
          }
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_pager_cb_event_compositor_resize(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Eina_List *l;
   Pager *p;

   EINA_LIST_FOREACH(pagers, l, p)
     {
        Eina_List *l2;
        Pager_Desk *pd;

        EINA_LIST_FOREACH(p->desks, l2, pd)
          e_layout_virtual_size_set(pd->o_layout, pd->desk->zone->w,
                                    pd->desk->zone->h);

        if (p->inst) _pager_orient(p->inst, e_gadget_site_orient_get(e_gadget_site_get(p->inst->o_pager)));
        /* TODO if (p->popup) */
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_cb_event_powersave_change(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   if (!pager_config->plain)
     return ECORE_CALLBACK_PASS_ON;

   _pager_cb_config_gadget_updated(EINA_TRUE);

   return ECORE_CALLBACK_PASS_ON;
}

static void
_pager_window_move(Pager_Win *pw)
{
   e_layout_child_move(pw->o_window,
                       pw->client->x - pw->client->zone->x,
                       pw->client->y - pw->client->zone->y);
   e_layout_child_resize(pw->o_window, pw->client->w, pw->client->h);
}

static void
_pager_window_cb_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Pager_Win *pw = data;

   pw->desk->wins = eina_list_remove(pw->desk->wins, pw);
   _pager_window_free(data);
}

static void
_pager_window_cb_mouse_up(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Evas_Event_Mouse_Up *ev = event_info;
   Pager_Win *pw = data;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (_pager_check_modifiers(ev->modifiers)) return;

   evas_object_smart_callback_call(e_gadget_site_get(pw->desk->pager->inst->o_pager), "gadget_site_unlocked", NULL);

   pw->drag.button = 0;
}

static void
_pager_window_cb_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Evas_Event_Mouse_Down *ev = event_info;
   Pager_Win *pw;
   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (_pager_check_modifiers(ev->modifiers)) return;

   pw = data;

   if (!pw) return;
   pw->desk->pager->active_drag_client = NULL;
   if (pw->desk->pager->popup && !act_popup) return;
   if (!pw->desk->pager->popup && ev->button == 3) return;
   if (e_client_util_ignored_get(pw->client) || e_client_util_is_popup(pw->client)) return;
   if (ev->button == (int)pager_config->btn_desk) return;
   if ((ev->button == (int)pager_config->btn_drag) ||
       (ev->button == (int)pager_config->btn_noplace))
     {
        Evas_Coord ox, oy;
        if ((pw->desk->pager->plain) || (pager_config->permanent_plain))
          evas_object_geometry_get(pw->o_window, &ox, &oy, NULL, NULL);
        else
          evas_object_geometry_get(pw->o_mirror, &ox, &oy, NULL, NULL);
        pw->drag.in_pager = 1;
        pw->drag.x = ev->canvas.x;
        pw->drag.y = ev->canvas.y;
        pw->drag.dx = ox - ev->canvas.x;
        pw->drag.dy = oy - ev->canvas.y;
        pw->drag.start = 1;
        pw->drag.button = ev->button;
        pw->desk->pager->active_drag_client = pw->client;
        evas_object_smart_callback_call(e_gadget_site_get(pw->desk->pager->inst->o_pager), "gadget_site_locked", NULL);
     }
}

static void
_pager_window_cb_mouse_move(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Evas_Event_Mouse_Move *ev = event_info;
   Pager_Win *pw;
   E_Drag *drag;
   Evas_Object *o, *oo = NULL;
   Evas_Coord x, y, w, h;
   const char *drag_types[] =
   { "enlightenment/pager_win", "enlightenment/border" };
   Evas_Coord dx, dy;
   unsigned int resist = 0;
   pw = data;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (_pager_check_modifiers(ev->modifiers)) return;
   if (!pw) return;
   if (pw->client->lock_user_location) return;
   if ((pw->desk->pager->popup) && (!act_popup)) return;
   if (!pw->drag.button) return;

   /* prevent drag for a few pixels */
   if (!pw->drag.start) return;

   dx = pw->drag.x - ev->cur.output.x;
   dy = pw->drag.y - ev->cur.output.y;
   if (pw->desk->pager)
     resist = pager_config->drag_resist;

   if (((unsigned int)(dx * dx) + (unsigned int)(dy * dy)) <=
       (resist * resist)) return;

   pw->desk->pager->dragging = 1;
   pw->drag.start = 0;
   e_comp_object_effect_clip(pw->client->frame);
   edje_object_signal_emit(pw->desk->o_desk, "e,action,drag,in", "e");

   if ((pw->desk->pager->plain) || (pager_config->permanent_plain))
     {
        evas_object_geometry_get(pw->o_window, &x, &y, &w, &h);
        evas_object_hide(pw->o_window);
     }
   else
     {
        evas_object_geometry_get(pw->o_mirror, &x, &y, &w, &h);
        evas_object_hide(pw->o_mirror);
     }
   drag = e_drag_new(x, y, drag_types, 2, pw->desk->pager, -1,
                     _pager_window_cb_drag_convert,
                     _pager_window_cb_drag_finished);
   drag->button_mask = evas_pointer_button_down_mask_get(e_comp->evas);
   if ((pw->desk->pager->plain) || (pager_config->permanent_plain))
     {
        o = edje_object_add(drag->evas);
        e_theme_edje_object_set(o, "base/theme/gadgets/pager/plain",
                                "e/gadget/pager/plain/window");
        evas_object_show(o);

        oo = e_client_icon_add(pw->client, drag->evas);
        if (oo)
          {
             evas_object_show(oo);
             edje_object_part_swallow(o, "e.swallow.icon", oo);
          }
     }
   else
     {
        /* this is independent of the original mirror */
        o = e_deskmirror_mirror_copy(pw->o_mirror);
        evas_object_show(o);
     }
   e_drag_object_set(drag, o);
   e_drag_resize(drag, w, h);
   e_drag_show(drag);
   e_drag_start(drag, x - pw->drag.dx, y - pw->drag.dy);
   if (oo)
     e_comp_object_util_del_list_append(drag->comp_object, oo);
}

static void *
_pager_window_cb_drag_convert(E_Drag *drag, const char *type)
{
   Pager *p;

   p = drag->data;
   if (!strcmp(type, "enlightenment/pager_win")) return _pager_window_find(p, p->active_drag_client);
   if (!strcmp(type, "enlightenment/border")) return p->active_drag_client;
   return NULL;
}

static void
_pager_window_cb_drag_finished(E_Drag *drag, int dropped)
{
   Pager_Win *pw;
   Pager *p;
   p = drag->data;
   if (!p) return;
   pw = _pager_window_find(p, p->active_drag_client);
   if (!pw) return;
   p->active_drag_client = NULL;
   if ((p->plain) || (pager_config->permanent_plain))
     evas_object_show(pw->o_window);
   else
     evas_object_show(pw->o_mirror);
   evas_object_smart_callback_call(e_gadget_site_get(pw->desk->pager->inst->o_pager), "gadget_site_unlocked", NULL);
   if (dropped)
     {
        /* be helpful */
        if (pw->client->desk->visible && (!e_client_focused_get()))
          evas_object_focus_set(pw->client->frame, 1);
     }
   else
     {
        int dx, dy, x, y, zx, zy, zw, zh;
        E_Client *ec = pw->client;

        ec->hidden = !p->active_pd->desk->visible;
        e_client_desk_set(ec, p->active_pd->desk);

        dx = (ec->w / 2);
        dy = (ec->h / 2);

        evas_pointer_canvas_xy_get(evas_object_evas_get(p->o_table), &x, &y);
        e_zone_useful_geometry_get(p->zone, &zx, &zy, &zw, &zh);

        /* offset so that center of window is on mouse, but keep within desk bounds */
        if (dx < x)
          {
             x -= dx;
             if ((ec->w < zw) && (x + ec->w > zx + zw))
               x -= x + ec->w - (zx + zw);
          }
        else x = 0;

        if (dy < y)
          {
             y -= dy;
             if ((ec->h < zh) && (y + ec->h > zy + zh))
               y -= y + ec->h - (zy + zh);
          }
        else y = 0;
        evas_object_move(ec->frame, x, y);

        if (!(ec->lock_user_stacking)) evas_object_raise(ec->frame);
        evas_object_focus_set(ec->frame, 1);
     }
   edje_object_signal_emit(pw->desk->o_desk, "e,action,drag,out", "e");
   if (!pw->drag.from_pager)
     {
        if (!pw->drag.start) p->just_dragged = 1;
        pw->drag.in_pager = 0;
        pw->drag.button = pw->drag.start = 0;
        p->dragging = 0;
     }
   if (pw->drag.from_pager) pw->drag.from_pager->dragging = 0;
   pw->drag.from_pager = NULL;
   e_comp_object_effect_unclip(pw->client->frame);
   if (act_popup)
     {
        if (e_comp->comp_type == E_PIXMAP_TYPE_X)
          e_grabinput_get(input_window, 0, input_window);
        else
          e_comp_grab_input(1, 1);
        if (!hold_count) _pager_popup_hide(1);
     }
}

static void
_pager_update_drop_position(Pager *p, Pager_Desk *pd, Evas_Coord x, Evas_Coord y)
{
   Pager_Win *pw = NULL;

   if ((p->plain) || (pager_config->permanent_plain))
     {
        if (pd)
          edje_object_signal_emit(pd->o_desk, "e,action,drag,in", "e");
        return;
     }
   if (pd)
     pw = _pager_desk_window_find(pd, p->active_drag_client);
   else
     pw = _pager_window_find(p, p->active_drag_client);
   if (!pw) return;
   if (pd)
     {
        int zx, zy, zw, zh, vx, vy;
        E_Client *ec = pw->client;
        E_Desk *old_desk = ec->desk;
        Eina_Bool was_focused = e_client_stack_focused_get(ec);

        pw->drag.in_pager = 1;
        //makes drags look weird
        //e_zone_useful_geometry_get(pd->desk->zone, &zx, &zy, &zw, &zh);
        zx = pd->desk->zone->x, zy = pd->desk->zone->y;
        zw = pd->desk->zone->w, zh = pd->desk->zone->h;
        e_deskmirror_coord_canvas_to_virtual(pd->o_layout,
                                             x + pw->drag.dx,
                                             y + pw->drag.dy, &vx, &vy);
        ec->hidden = !pd->desk->visible;
        e_client_desk_set(ec, pd->desk);
        x = E_CLAMP(vx + zx, zx, zx + zw - ec->w);
        y = E_CLAMP(vy + zy, zy, zy + zh - ec->h);
        evas_object_move(ec->frame, x, y);
        if (was_focused)
          e_desk_last_focused_focus(old_desk);
     }
   else
     {
        /* this prevents the desk from switching on drags */
        pw->drag.from_pager = pw->desk->pager;
        pw->drag.from_pager->dragging = 1;
        pw->drag.in_pager = 0;
     }
}

static void
_pager_drop_cb_enter(void *data, const char *type EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Pager_Desk *pd = data;

   /* FIXME this fixes a segv, but the case is not easy
    * reproduceable. this makes no sense either since
    * the same 'pager' is passed to e_drop_handler_add
    * and it works without this almost all the time.
    * so this must be an issue with e_dnd code... i guess */
   if (act_popup) return;
   edje_object_signal_emit(pd->o_desk, "e,action,drag,in", "e");
}

static void
_pager_drop_cb_move(void *data, const char *type EINA_UNUSED, void *event_info)
{
   E_Event_Dnd_Move *ev;
   Pager_Desk *pd;

   ev = event_info;
   pd = data;

   if (act_popup) return;
   _pager_update_drop_position(pd->pager, pd, ev->x, ev->y);
}

static void
_pager_drop_cb_leave(void *data, const char *type EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Pager_Desk *pd = data;

   if (act_popup) return;
   edje_object_signal_emit(pd->o_desk, "e,action,drag,out", "e");
}

static void
_pager_drop_cb_drop(void *data, const char *type, void *event_info)
{
   E_Event_Dnd_Drop *ev;
   Eina_List *l;
   Pager_Desk *pd, *pdd;
   Pager_Desk *pd2 = NULL;
   E_Client *ec = NULL;
   int dx = 0, dy = 0;
   Pager_Win *pw = NULL;
   Evas_Coord wx, wy, wx2, wy2;
   Evas_Coord nx, ny;
   ev = event_info;
   pd = data;

   if (act_popup) return;

   if (pd)
     {
        if (!strcmp(type, "enlightenment/pager_win"))
          {
             pw = (Pager_Win *)(ev->data);
             if (pw)
               {
                  ec = pw->client;
                  dx = pw->drag.dx;
                  dy = pw->drag.dy;
               }
          }
        else if (!strcmp(type, "enlightenment/border"))
          {
             ec = ev->data;
             if ((pd->pager->plain) || (pager_config->permanent_plain))
               {
                  e_layout_coord_virtual_to_canvas(pd->o_layout, ec->x, ec->y,
                                                   &wx, &wy);
                  e_layout_coord_virtual_to_canvas(pd->o_layout, ec->x + ec->w,
                                                   ec->y + ec->h, &wx2, &wy2);
               }
             else
               {
                  e_deskmirror_coord_virtual_to_canvas(pd->o_layout, ec->x, ec->y,
                                                       &wx, &wy);
                  e_deskmirror_coord_virtual_to_canvas(pd->o_layout, ec->x + ec->w,
                                                       ec->y + ec->h, &wx2, &wy2);
               }
             dx = (wx - wx2) / 2;
             dy = (wy - wy2) / 2;
          }
        else if (!strcmp(type, "enlightenment/vdesktop"))
          {
             pd2 = ev->data;
             if (!pd2) return;
             _pager_desk_switch(pd, pd2);
          }
        else
          return;

        if (ec)
          {
             E_Maximize max = ec->maximized;
             E_Fullscreen fs = ec->fullscreen_policy;
             Eina_Bool fullscreen = ec->fullscreen;
             E_Desk *old_desk = ec->desk;
             Eina_Bool was_focused = e_client_stack_focused_get(ec);

             if (ec->iconic) e_client_uniconify(ec);
             if (ec->maximized)
               e_client_unmaximize(ec, E_MAXIMIZE_BOTH);
             if (fullscreen) e_client_unfullscreen(ec);
             ec->hidden = 0;
             e_client_desk_set(ec, pd->desk);
             if (was_focused)
               e_desk_last_focused_focus(old_desk);
             evas_object_raise(ec->frame);

             if ((!max) && (!fullscreen))
               {
                  int zx, zy, zw, zh, mx, my;
                  if ((pd->pager->plain) || (pager_config->permanent_plain))
                    {
                       e_layout_coord_canvas_to_virtual(pd->o_layout, ev->x + dx, ev->y + dy,
                                                        &nx, &ny);
                    }
                  else
                    {
                       e_deskmirror_coord_canvas_to_virtual(pd->o_layout,
                                                            ev->x + dx,
                                                            ev->y + dy,
                                                            &nx, &ny);
                    }
                  e_zone_useful_geometry_get(pd->desk->zone,
                                             &zx, &zy, &zw, &zh);

                  mx = E_CLAMP(nx + zx, zx, zx + zw - ec->w);
                  my = E_CLAMP(ny + zy, zy, zy + zh - ec->h);
                  evas_object_move(ec->frame, mx, my);
               }
             if (max) e_client_maximize(ec, max);
             if (fullscreen) e_client_fullscreen(ec, fs);
          }
        EINA_LIST_FOREACH(pd->pager->desks, l, pdd)
          {
             edje_object_signal_emit(pdd->o_desk, "e,action,drag,out", "e");
          }
     }
}

static void
_pager_desk_cb_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Evas_Event_Mouse_Down *ev = event_info;
   Pager_Desk *pd;
   Evas_Coord ox, oy;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (_pager_check_modifiers(ev->modifiers)) return;

   pd = data;
   if (!pd) return;
   if ((!pd->pager->popup) && (ev->button == 3)) return;
   if (ev->button == (int)pager_config->btn_desk)
     {
        evas_object_geometry_get(pd->o_desk, &ox, &oy, NULL, NULL);
        pd->drag.start = 1;
        pd->drag.in_pager = 1;
        pd->drag.dx = ox - ev->canvas.x;
        pd->drag.dy = oy - ev->canvas.y;
        pd->drag.x = ev->canvas.x;
        pd->drag.y = ev->canvas.y;
        pd->drag.button = ev->button;
        evas_object_smart_callback_call(e_gadget_site_get(pd->pager->inst->o_pager), "gadget_site_locked", NULL);
     }
   else
     {
        pd->drag.dx = pd->drag.dy = pd->drag.x = pd->drag.y = 0;
     }
   pd->pager->just_dragged = 0;
}

static void
_pager_desk_cb_mouse_up(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Evas_Event_Mouse_Up *ev = event_info;
   Pager_Desk *pd;
   Pager *p;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (_pager_check_modifiers(ev->modifiers)) return;

   pd = data;

   if (!pd) return;
   p = pd->pager;

   /* FIXME: pd->pager->dragging is 0 when finishing a drag from desk to desk */
   if ((ev->button == 1) && (!pd->pager->dragging) &&
       (!pd->pager->just_dragged))
     {
        current_desk = pd->desk;
        e_desk_show(pd->desk);
        pd->drag.start = 0;
        pd->drag.in_pager = 0;
     }
   else if (ev->button == (int)pager_config->btn_desk)
     {
        if (pd->pager->dragging) pd->pager->dragging = 0;
        pd->drag.start = 0;
        pd->drag.in_pager = 0;
        evas_object_smart_callback_call(e_gadget_site_get(p->inst->o_pager), "gadget_site_unlocked", NULL);
     }

   if ((p->popup) && (p->popup->urgent)) _pager_popup_free(p->popup);
}

static void
_pager_desk_cb_mouse_move(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Evas_Event_Mouse_Move *ev = event_info;
   Pager_Desk *pd;
   Evas_Coord dx, dy;
   unsigned int resist = 0;
   E_Drag *drag;
   Evas_Object *o;
   Evas_Coord x, y, w, h;
   const char *drag_types[] = { "enlightenment/vdesktop" };

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (_pager_check_modifiers(ev->modifiers)) return;

   pd = data;
   if (!pd) return;
   /* prevent drag for a few pixels */
   if (pd->drag.start)
     {
        dx = pd->drag.x - ev->cur.output.x;
        dy = pd->drag.y - ev->cur.output.y;
        if ((pd->pager) && (pd->pager->inst))
          resist = pager_config->drag_resist;

        if (((unsigned int)(dx * dx) + (unsigned int)(dy * dy)) <=
            (resist * resist)) return;

        if (pd->pager) pd->pager->dragging = 1;
        pd->drag.start = 0;
     }

   if (pd->drag.in_pager && pd->pager)
     {
        evas_object_geometry_get(pd->o_desk, &x, &y, &w, &h);
        drag = e_drag_new(x, y, drag_types, 1, pd, -1,
                          NULL, _pager_desk_cb_drag_finished);
        drag->button_mask = evas_pointer_button_down_mask_get(e_comp->evas);
        /* redraw the desktop theme above */
        if ((pd->pager->plain) || (pager_config->permanent_plain))
          {
             o = edje_object_add(drag->evas);
             e_theme_edje_object_set(o, "base/theme/gadgets/pager",
                                     "e/gadget/pager/plain/desk");
             evas_object_show(o);
          }
        else
          {
             o = e_comp_object_util_mirror_add(pd->o_layout);
          }
        e_drag_object_set(drag, o);

        e_drag_resize(drag, w, h);
        e_drag_start(drag, x - pd->drag.dx, y - pd->drag.dy);

        pd->drag.from_pager = pd->pager;
        pd->drag.from_pager->dragging = 1;
        pd->drag.in_pager = 0;
     }
}

static void
_pager_desk_cb_drag_finished(E_Drag *drag, int dropped)
{
   Pager_Desk *pd;
   Pager_Desk *pd2 = NULL;
   Eina_List *l;
   E_Desk *desk;
   E_Zone *zone;
   Pager *p;

   pd = drag->data;
   if (!pd) return;
   evas_object_smart_callback_call(e_gadget_site_get(pd->pager->inst->o_pager), "gadget_site_unlocked", NULL);
   if (!dropped)
     {
        /* wasn't dropped on pager, switch with current desktop */
        if (!pd->desk) return;
        zone = e_zone_current_get();
        desk = e_desk_current_get(zone);
        EINA_LIST_FOREACH(pagers, l, p)
          {
             pd2 = _pager_desk_find(p, desk);
             if (pd2) break;
          }
        _pager_desk_switch(pd, pd2);
     }
   if (pd->drag.from_pager)
     {
        pd->drag.from_pager->dragging = 0;
        pd->drag.from_pager->just_dragged = 0;
     }
   edje_object_signal_emit(pd->o_desk, "e,action,drag,out", "e");
   pd->drag.from_pager = NULL;

   if (act_popup)
     {
        if (e_comp->comp_type == E_PIXMAP_TYPE_X)
          e_grabinput_get(input_window, 0, input_window);
        else
          e_comp_grab_input(1, 1);
        if (!hold_count) _pager_popup_hide(1);
     }
}

static void
_pager_desk_cb_mouse_wheel(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Evas_Event_Mouse_Wheel *ev = event_info;
   Pager_Desk *pd;

   if (ev->event_flags & EVAS_EVENT_FLAG_ON_HOLD) return;
   if (_pager_check_modifiers(ev->modifiers)) return;

   pd = data;

   if (pd->pager->popup) return;

   if (pager_config->flip_desk)
     e_zone_desk_linear_flip_by(pd->desk->zone, ev->z);
}

static Eina_Bool
_pager_popup_cb_timeout(void *data)
{
   Pager_Popup *pp;

   pp = data;
   pp->timer = NULL;
   _pager_popup_free(pp);

#ifndef HAVE_WAYLAND_ONLY
   if (e_comp->comp_type == E_PIXMAP_TYPE_X)
     {
        if (input_window)
          {
             e_grabinput_release(input_window, input_window);
             ecore_x_window_free(input_window);
             input_window = 0;
          }
     }
#endif
   if (e_comp->comp_type == E_PIXMAP_TYPE_WL)
     {
        e_comp_ungrab_input(1, 1);
        input_window = 0;
     }

   return ECORE_CALLBACK_CANCEL;
}

/************************************************************************/
/* popup-on-keyaction functions */
static int
_pager_popup_show(void)
{
   E_Zone *zone;
   int x, y, w, h;
   Pager_Popup *pp;
   //const char *drop[] =
   //{
   //"enlightenment/pager_win", "enlightenment/border",
   //"enlightenment/vdesktop"
   //};

   if ((act_popup) || (input_window)) return 0;

   zone = e_zone_current_get();

   pp = _pager_popup_find(zone);
   if (pp) _pager_popup_free(pp);

#ifndef HAVE_WAYLAND_ONLY
   if (e_comp->comp_type == E_PIXMAP_TYPE_X)
     {
        input_window = ecore_x_window_input_new(e_comp->win, 0, 0, 1, 1);
        ecore_x_window_show(input_window);
        if (!e_grabinput_get(input_window, 0, input_window))
          {
             ecore_x_window_free(input_window);
             input_window = 0;
             return 0;
          }
     }
#endif
   if (e_comp->comp_type == E_PIXMAP_TYPE_WL)
     {
        input_window = e_comp->ee_win;
        e_comp_grab_input(1, 1);
     }

   handlers = eina_list_append
       (handlers, ecore_event_handler_add
         (ECORE_EVENT_KEY_DOWN, _pager_popup_cb_key_down, NULL));
   handlers = eina_list_append
       (handlers, ecore_event_handler_add
         (ECORE_EVENT_KEY_UP, _pager_popup_cb_key_up, NULL));
   handlers = eina_list_append
       (handlers, ecore_event_handler_add
         (ECORE_EVENT_MOUSE_WHEEL, _pager_popup_cb_mouse_wheel, NULL));

   act_popup = pager_popup_new(0);

   evas_object_geometry_get(act_popup->pager->o_table, &x, &y, &w, &h);

   current_desk = e_desk_current_get(zone);

   return 1;
}

static void
_pager_popup_hide(int switch_desk)
{
   hold_count = 0;
   hold_mod = 0;
   while (handlers)
     {
        ecore_event_handler_del(handlers->data);
        handlers = eina_list_remove_list(handlers, handlers);
     }

   act_popup->timer = ecore_timer_loop_add(0.1, _pager_popup_cb_timeout, act_popup);

   if ((switch_desk) && (current_desk)) e_desk_show(current_desk);

   act_popup = NULL;
}

static void
_pager_popup_modifiers_set(int mod)
{
   if (!act_popup) return;
   hold_mod = mod;
   hold_count = 0;
   if (hold_mod & ECORE_EVENT_MODIFIER_SHIFT) hold_count++;
   if (hold_mod & ECORE_EVENT_MODIFIER_CTRL) hold_count++;
   if (hold_mod & ECORE_EVENT_MODIFIER_ALT) hold_count++;
   if (hold_mod & ECORE_EVENT_MODIFIER_WIN) hold_count++;
}

static void
_pager_popup_desk_switch(int x, int y)
{
   int max_x, max_y, desk_x, desk_y;
   Pager_Desk *pd;
   Pager_Popup *pp = act_popup;

   e_zone_desk_count_get(pp->pager->zone, &max_x, &max_y);

   desk_x = current_desk->x + x;
   desk_y = current_desk->y + y;

   if (desk_x < 0)
     desk_x = max_x - 1;
   else if (desk_x >= max_x)
     desk_x = 0;

   if (desk_y < 0)
     desk_y = max_y - 1;
   else if (desk_y >= max_y)
     desk_y = 0;

   current_desk = e_desk_at_xy_get(pp->pager->zone, desk_x, desk_y);

   pd = _pager_desk_find(pp->pager, current_desk);
   if (pd) _pager_desk_select(pd);

   edje_object_part_text_set(pp->o_bg, "e.text.label", current_desk->name);
}

static void
_pager_popup_cb_action_show(E_Object *obj EINA_UNUSED, const char *params EINA_UNUSED, Ecore_Event_Key *ev EINA_UNUSED)
{
   if (_pager_popup_show())
     _pager_popup_modifiers_set(ev->modifiers);
}

static void
_pager_popup_cb_action_switch(E_Object *obj EINA_UNUSED, const char *params, Ecore_Event_Key *ev)
{
   int max_x, max_y, desk_x;
   int x = 0, y = 0;

   if (!act_popup)
     {
        if (_pager_popup_show())
          _pager_popup_modifiers_set(ev->modifiers);
        else
          return;
     }

   e_zone_desk_count_get(act_popup->pager->zone, &max_x, &max_y);
   desk_x = current_desk->x /* + x <=this is always 0 */;

   if (!strcmp(params, "left"))
     x = -1;
   else if (!strcmp(params, "right"))
     x = 1;
   else if (!strcmp(params, "up"))
     y = -1;
   else if (!strcmp(params, "down"))
     y = 1;
   else if (!strcmp(params, "next"))
     {
        x = 1;
        if (desk_x == max_x - 1)
          y = 1;
     }
   else if (!strcmp(params, "prev"))
     {
        x = -1;
        if (desk_x == 0)
          y = -1;
     }

   _pager_popup_desk_switch(x, y);
}

static Eina_Bool
_pager_popup_cb_mouse_wheel(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Event_Mouse_Wheel *ev = event;
   Pager_Popup *pp = act_popup;
   int max_x;

   e_zone_desk_count_get(pp->pager->zone, &max_x, NULL);

   if (current_desk->x + ev->z >= max_x)
     _pager_popup_desk_switch(1, 1);
   else if (current_desk->x + ev->z < 0)
     _pager_popup_desk_switch(-1, -1);
   else
     _pager_popup_desk_switch(ev->z, 0);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_popup_cb_key_down(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Event_Key *ev;

   ev = event;
   if (ev->window != input_window) return ECORE_CALLBACK_PASS_ON;
   if (!strcmp(ev->key, "Up"))
     _pager_popup_desk_switch(0, -1);
   else if (!strcmp(ev->key, "Down"))
     _pager_popup_desk_switch(0, 1);
   else if (!strcmp(ev->key, "Left"))
     _pager_popup_desk_switch(-1, 0);
   else if (!strcmp(ev->key, "Right"))
     _pager_popup_desk_switch(1, 0);
   else if (!strcmp(ev->key, "Escape"))
     _pager_popup_hide(0);
   else if ((!strcmp(ev->key, "Return")) || (!strcmp(ev->key, "KP_Enter")) ||
            (!strcmp(ev->key, "space")))
     {
        Pager_Popup *pp = act_popup;

        if (pp)
          {
             E_Desk *desk;

             desk = e_desk_at_xy_get(pp->pager->zone,
                                     current_desk->x, current_desk->y);
             if (desk) e_desk_show(desk);
          }
        _pager_popup_hide(0);
     }
   else
     {
        E_Config_Binding_Key *binding;
        Eina_List *l;

        EINA_LIST_FOREACH(e_bindings->key_bindings, l, binding)
          {
             E_Binding_Modifier mod = 0;

             if ((binding->action) && (strcmp(binding->action, "pager_gadget_switch")))
               continue;

             if (ev->modifiers & ECORE_EVENT_MODIFIER_SHIFT)
               mod |= E_BINDING_MODIFIER_SHIFT;
             if (ev->modifiers & ECORE_EVENT_MODIFIER_CTRL)
               mod |= E_BINDING_MODIFIER_CTRL;
             if (ev->modifiers & ECORE_EVENT_MODIFIER_ALT)
               mod |= E_BINDING_MODIFIER_ALT;
             if (ev->modifiers & ECORE_EVENT_MODIFIER_WIN)
               mod |= E_BINDING_MODIFIER_WIN;

             if (binding->key && (!strcmp(binding->key, ev->key)) &&
                 ((binding->modifiers == mod)))
               {
                  E_Action *act;

                  act = e_action_find(binding->action);

                  if (act)
                    {
                       if (act->func.go_key)
                         act->func.go_key(NULL, binding->params, ev);
                    }
               }
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pager_popup_cb_key_up(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Event_Key *ev;

   ev = event;
   if (!(act_popup)) return ECORE_CALLBACK_PASS_ON;

   if (hold_mod)
     {
        if ((hold_mod & ECORE_EVENT_MODIFIER_SHIFT) &&
            (!strcmp(ev->key, "Shift_L"))) hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_SHIFT) &&
                 (!strcmp(ev->key, "Shift_R")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_CTRL) &&
                 (!strcmp(ev->key, "Control_L")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_CTRL) &&
                 (!strcmp(ev->key, "Control_R")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_ALT) &&
                 (!strcmp(ev->key, "Alt_L")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_ALT) &&
                 (!strcmp(ev->key, "Alt_R")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_ALT) &&
                 (!strcmp(ev->key, "Meta_L")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_ALT) &&
                 (!strcmp(ev->key, "Meta_R")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_ALT) &&
                 (!strcmp(ev->key, "Super_L")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_ALT) &&
                 (!strcmp(ev->key, "Super_R")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_WIN) &&
                 (!strcmp(ev->key, "Super_L")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_WIN) &&
                 (!strcmp(ev->key, "Super_R")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_WIN) &&
                 (!strcmp(ev->key, "Mode_switch")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_WIN) &&
                 (!strcmp(ev->key, "Meta_L")))
          hold_count--;
        else if ((hold_mod & ECORE_EVENT_MODIFIER_WIN) &&
                 (!strcmp(ev->key, "Meta_R")))
          hold_count--;
        if ((hold_count <= 0) && (!act_popup->pager->dragging))
          {
             _pager_popup_hide(1);
             return ECORE_CALLBACK_PASS_ON;
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static void
pager_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance *inst = data;

   _pager_free(inst->pager);
   ginstances = eina_list_remove(ginstances, inst);
   free(inst);
}

EINTERN Evas_Object *
pager_create(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient EINA_UNUSED)
{
   Pager *p;
   Evas_Object *o;
   Instance *inst;

   if (e_gadget_site_is_desklock(parent)) return NULL;
   if (*id == 0) *id = 1;
   inst = E_NEW(Instance, 1);
   p = _pager_new(evas_object_evas_get(parent), EINA_FALSE);
   p->inst = inst;
   inst->pager = p;
   o = p->o_table;
   inst->o_pager = o;
   _pager_orient(inst, e_gadget_site_orient_get(parent));

   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, pager_del, inst);
   evas_object_smart_callback_add(parent, "gadget_created", _pager_gadget_created_cb, inst);
   evas_object_smart_callback_add(parent, "gadget_site_anchor", _pager_gadget_anchor_change_cb, inst);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOVE, _pager_cb_move, inst);
   evas_object_event_callback_add(o, EVAS_CALLBACK_MOUSE_DOWN,
                                  _button_cb_mouse_down, inst);
   evas_object_event_callback_add(o, EVAS_CALLBACK_SHOW,
                                  _pager_cb_obj_show, inst);
   evas_object_event_callback_add(o, EVAS_CALLBACK_HIDE,
                                  _pager_cb_obj_hide, inst);
   ginstances = eina_list_append(ginstances, inst);
   return inst->o_pager;
}

EINTERN void
pager_init(void)
{
   E_LIST_HANDLER_APPEND(ghandlers, E_EVENT_ZONE_DESK_COUNT_SET, _pager_cb_event_zone_desk_count_set, NULL);
   E_LIST_HANDLER_APPEND(ghandlers, E_EVENT_DESK_SHOW, _pager_cb_event_desk_show, NULL);
   E_LIST_HANDLER_APPEND(ghandlers, E_EVENT_DESK_NAME_CHANGE, _pager_cb_event_desk_name_change, NULL);
   E_LIST_HANDLER_APPEND(ghandlers, E_EVENT_COMPOSITOR_UPDATE, _pager_cb_event_compositor_resize, NULL);
   E_LIST_HANDLER_APPEND(ghandlers, E_EVENT_CLIENT_PROPERTY, _pager_cb_event_client_urgent_change, NULL);
   E_LIST_HANDLER_APPEND(ghandlers, E_EVENT_POWERSAVE_UPDATE, _pager_cb_event_powersave_change, NULL);

   act_popup_show = e_action_add("pager_gadget_show");
   if (act_popup_show)
     {
        act_popup_show->func.go_key = _pager_popup_cb_action_show;
        e_action_predef_name_set(N_("Pager Gadget"), N_("Show Pager Popup"),
                                 "pager_gadget_show", "<none>", NULL, 0);
     }
   act_popup_switch = e_action_add("pager_gadget_switch");
   if (act_popup_switch)
     {
        act_popup_switch->func.go_key = _pager_popup_cb_action_switch;
        e_action_predef_name_set(N_("Pager Gadget"), N_("Popup Desk Right"),
                                 "pager_gadget_switch", "right", NULL, 0);
        e_action_predef_name_set(N_("Pager Gadget"), N_("Popup Desk Left"),
                                 "pager_gadget_switch", "left", NULL, 0);
        e_action_predef_name_set(N_("Pager Gadget"), N_("Popup Desk Up"),
                                 "pager_gadget_switch", "up", NULL, 0);
        e_action_predef_name_set(N_("Pager Gadget"), N_("Popup Desk Down"),
                                 "pager_gadget_switch", "down", NULL, 0);
        e_action_predef_name_set(N_("Pager Gadget"), N_("Popup Desk Next"),
                                 "pager_gadget_switch", "next", NULL, 0);
        e_action_predef_name_set(N_("Pager Gadget"), N_("Popup Desk Previous"),
                                 "pager_gadget_switch", "prev", NULL, 0);
     }
}

