#ifndef E_TYPEDEFS
#ifndef E_GADGET_H
# define E_GADGET_H


/** SMART CALLBACKS:
 -------------------------------
 * called by gadget site internals on gadget site object:

 * have a gadget object as event_info
 * {
      "gadget_added"
       - a new gadget was added to the gadget site by the user
      "gadget_created"
       - a gadget object was created on the site
      "gadget_destroyed"
       - a gadget object was destroyed on the site; all objects created by the
         gadget infrastructure are now dead
      "gadget_moved"
       - a gadget is preparing to move from its current site
      "gadget_removed"
       - a gadget was removed from the gadget site by the user
       - the gadget should remove its config when this is triggered
 * }

 * have NULL as event_info
 * {
      "gadget_site_anchor"
       - the anchor of the gadget site changed
      "gadget_site_gravity"
       - the gravity of the gadget site changed
 * }

 * have E_Menu as event_info
 * {
      "gadget_site_owner_menu"
       - the owner (parent object) of the site should add any owner-specific items
       in this callback (eg. settings)
      "gadget_site_style_menu"
       - the owner (parent object) of the site should add any style-specific items
       in this callback (eg. plain, inset)
 * }

 * have Evas_Object as event_info
 * {
      "gadget_site_popup"
      - a popup has been triggered from the site; the site must remain visible until
      the passed popup object has been hidden
      - event_info is the Evas_Object of the popup
 * }
 -------------------------------
 -------------------------------
 * called externally on gadget site
   "gadget_site_dropped"
     - called on a target site when a gadget site is dropped on it
     - event_info is the dropped site
     - all gadgets on the dropped site will be moved to the target site
   "gadget_site_style"
     - called on a target site when its owner has executed a style change
     - event_info is NULL
     - triggers restyling of all contained gadgets
 -------------------------------
 -------------------------------
 * called by gadget internals on gadget object:
   "gadget_menu"
     - called on a gadget object when the "gadget_menu" action has been triggered
     - event_info is an E_Menu object
     - if a configure callback has been passed with e_gadget_configure_cb_set(),
     a "Settings" item will be automatically added with this callback
 -------------------------------
 -------------------------------
 * called externally by gadget on gadget object:
   "gadget_popup"
     - called on a gadget object by the gadget when the gadget creates a popup which
     requires that the gadget remain visible for the lifetime of the popup
     - event_info is the Evas_Object of the popup
 */

typedef enum
{
   E_GADGET_SITE_GRAVITY_NONE = 0,
   E_GADGET_SITE_GRAVITY_LEFT,
   E_GADGET_SITE_GRAVITY_RIGHT,
   E_GADGET_SITE_GRAVITY_TOP,
   E_GADGET_SITE_GRAVITY_BOTTOM,
   E_GADGET_SITE_GRAVITY_CENTER,
} E_Gadget_Site_Gravity;

typedef enum
{
   E_GADGET_SITE_ORIENT_NONE = 0,
   E_GADGET_SITE_ORIENT_HORIZONTAL,
   E_GADGET_SITE_ORIENT_VERTICAL,
} E_Gadget_Site_Orient;

typedef enum
{
   E_GADGET_SITE_ANCHOR_NONE = 0,
   E_GADGET_SITE_ANCHOR_LEFT = (1 << 0),
   E_GADGET_SITE_ANCHOR_RIGHT = (1 << 1),
   E_GADGET_SITE_ANCHOR_TOP = (1 << 2),
   E_GADGET_SITE_ANCHOR_BOTTOM = (1 << 3),
} E_Gadget_Site_Anchor;

typedef Evas_Object *(*E_Gadget_Create_Cb)(Evas_Object *parent, int *id, E_Gadget_Site_Orient orient);
typedef Evas_Object *(*E_Gadget_Configure_Cb)(Evas_Object *gadget);
typedef void (*E_Gadget_Wizard_End_Cb)(void *data, int id);
typedef void (*E_Gadget_Wizard_Cb)(E_Gadget_Wizard_End_Cb cb, void *data);
typedef void (*E_Gadget_Style_Cb)(Evas_Object *owner, Eina_Stringshare *name, Evas_Object *g);

EINTERN void e_gadget_init(void);
EINTERN void e_gadget_shutdown(void);

E_API Evas_Object *e_gadget_site_add(E_Gadget_Site_Orient orient, const char *name);
E_API Evas_Object *e_gadget_site_auto_add(E_Gadget_Site_Orient orient, const char *name);
E_API void e_gadget_site_del(Evas_Object *obj);
E_API E_Gadget_Site_Anchor e_gadget_site_anchor_get(Evas_Object *obj);
E_API void e_gadget_site_owner_setup(Evas_Object *obj, E_Gadget_Site_Anchor an, E_Gadget_Style_Cb cb);
E_API E_Gadget_Site_Orient e_gadget_site_orient_get(Evas_Object *obj);
E_API E_Gadget_Site_Gravity e_gadget_site_gravity_get(Evas_Object *obj);
E_API void e_gadget_site_gravity_set(Evas_Object *obj, E_Gadget_Site_Gravity gravity);
E_API void e_gadget_site_gadget_add(Evas_Object *obj, const char *type, Eina_Bool demo);
E_API Eina_List *e_gadget_site_gadgets_list(Evas_Object *obj);

E_API void e_gadget_configure_cb_set(Evas_Object *g, E_Gadget_Configure_Cb cb);
E_API void e_gadget_configure(Evas_Object *g);
E_API Evas_Object *e_gadget_site_get(Evas_Object *g);
E_API Eina_Stringshare *e_gadget_type_get(Evas_Object *g);

E_API void e_gadget_type_add(const char *type, E_Gadget_Create_Cb callback, E_Gadget_Wizard_Cb wizard);
E_API void e_gadget_type_del(const char *type);
E_API Eina_Iterator *e_gadget_type_iterator_get(void);

E_API Evas_Object *e_gadget_util_layout_style_init(Evas_Object *g, Evas_Object *style);
E_API void e_gadget_util_ctxpopup_place(Evas_Object *g, Evas_Object *ctx, Evas_Object *pos_obj);
E_API void e_gadget_util_allow_deny_ctxpopup(Evas_Object *g, const char *text, Evas_Smart_Cb allow_cb, Evas_Smart_Cb deny_cb, const void *data);

E_API Evas_Object *e_gadget_editor_add(Evas_Object *parent, Evas_Object *site);
E_API Evas_Object *e_gadget_site_edit(Evas_Object *site);
E_API void e_gadget_site_desklock_edit(void);
#endif
#endif
