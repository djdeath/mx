/* mx-toggle.c */

#include "mx-toggle.h"
#include "mx-private.h"
#include "mx-stylable.h"


/* mx-toggle-handle */
#define MX_TYPE_TOGGLE_HANDLE mx_toggle_handle_get_type()

#define MX_TOGGLE_HANDLE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), MX_TYPE_TOGGLE_HANDLE, MxToggleHandle))

#define MX_IS_TOGGLE_HANDLE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MX_TYPE_TOGGLE_HANDLE))

typedef struct
{
  MxWidget parent;
} MxToggleHandle;

typedef struct
{
  MxWidgetClass parent_class;
} MxToggleHandleClass;

GType mx_toggle_handle_get_type (void) G_GNUC_CONST;

G_DEFINE_TYPE (MxToggleHandle, mx_toggle_handle, MX_TYPE_WIDGET)

static void
mx_toggle_background_get_preferred_width (ClutterActor *actor,
                                          gfloat        for_height,
                                          gfloat       *min_width_p,
                                          gfloat       *pref_width_p)
{
  ClutterActor *background;

  background = mx_widget_get_background_image (MX_WIDGET (actor));

  if (!background)
    {
      if (*min_width_p)
        min_width_p = 0;
      if (*pref_width_p)
        pref_width_p = 0;

      return;
    }

  clutter_actor_get_preferred_width (background, for_height, min_width_p,
                                     pref_width_p);
}

static void
mx_toggle_background_get_preferred_height (ClutterActor *actor,
                                           gfloat        for_width,
                                           gfloat       *min_height_p,
                                           gfloat       *pref_height_p)
{
  ClutterActor *background;

  background = mx_widget_get_background_image (MX_WIDGET (actor));

  if (!background)
    {
      if (*min_height_p)
        min_height_p = 0;
      if (*pref_height_p)
        pref_height_p = 0;

      return;
    }

  clutter_actor_get_preferred_height (background, for_width, min_height_p,
                                      pref_height_p);
}



static void
mx_toggle_handle_class_init (MxToggleHandleClass *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->get_preferred_width = mx_toggle_background_get_preferred_width;
  actor_class->get_preferred_height = mx_toggle_background_get_preferred_height;
}

static void
mx_toggle_handle_init (MxToggleHandle *self)
{
}

/* mx-toggle */

static void mx_toggle_stylable_iface_init (MxStylableIface *iface);

G_DEFINE_TYPE_WITH_CODE (MxToggle, mx_toggle, MX_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (MX_TYPE_STYLABLE,
                                                mx_toggle_stylable_iface_init))

#define TOGGLE_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MX_TYPE_TOGGLE, MxTogglePrivate))

struct _MxTogglePrivate
{
  gboolean active;

  ClutterActor *handle;
  gchar        *handle_filename;


  ClutterAlpha *alpha;
  gfloat        position;

  gfloat        drag_offset;
  gfloat        slide_length;
  gfloat        last_move;
};

enum
{
  PROP_ACTIVE = 1
};

static void
mx_toggle_stylable_iface_init (MxStylableIface *iface)
{
  GParamSpec *pspec;

  pspec = g_param_spec_string ("handle-image",
                               "Handle Image",
                               "Image used for the handle of the toggle",
                               "",
                               MX_PARAM_READWRITE);
  mx_stylable_iface_install_property (iface, MX_TYPE_TOGGLE, pspec);
}

static void
mx_toggle_get_property (GObject    *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  MxTogglePrivate *priv = MX_TOGGLE (object)->priv;

  switch (property_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, priv->active);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
mx_toggle_set_property (GObject      *object,
                        guint         property_id,
                        const GValue *value,
                        GParamSpec   *pspec)
{
  MxToggle *toggle = MX_TOGGLE (object);

  switch (property_id)
    {
    case PROP_ACTIVE:
      mx_toggle_set_active (toggle, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
mx_toggle_dispose (GObject *object)
{
  MxTogglePrivate *priv = MX_TOGGLE (object)->priv;

  if (priv->handle)
    {
      clutter_actor_destroy (priv->handle);
      priv->handle = NULL;
    }

  G_OBJECT_CLASS (mx_toggle_parent_class)->dispose (object);
}

static void
mx_toggle_finalize (GObject *object)
{
  G_OBJECT_CLASS (mx_toggle_parent_class)->finalize (object);
}

static void
mx_toggle_pick (ClutterActor       *actor,
                const ClutterColor *color)
{
  CLUTTER_ACTOR_CLASS (mx_toggle_parent_class)->pick (actor, color);

  clutter_actor_paint (MX_TOGGLE (actor)->priv->handle);
}

static void
mx_toggle_paint (ClutterActor *actor)
{
  CLUTTER_ACTOR_CLASS (mx_toggle_parent_class)->paint (actor);

  clutter_actor_paint (MX_TOGGLE (actor)->priv->handle);
}

static void
mx_toggle_map (ClutterActor *actor)
{
  MxTogglePrivate *priv = MX_TOGGLE (actor)->priv;

  CLUTTER_ACTOR_CLASS (mx_toggle_parent_class)->map (actor);

  clutter_actor_map (priv->handle);
}

static void
mx_toggle_unmap (ClutterActor *actor)
{
  MxTogglePrivate *priv = MX_TOGGLE (actor)->priv;

  clutter_actor_unmap (priv->handle);

  CLUTTER_ACTOR_CLASS (mx_toggle_parent_class)->unmap (actor);
}

static void
mx_toggle_allocate (ClutterActor          *actor,
                    const ClutterActorBox *box,
                    ClutterAllocationFlags flags)
{
  MxTogglePrivate *priv = MX_TOGGLE (actor)->priv;
  ClutterActorBox handle_box, child_box;
  gfloat handle_w, handle_h;
  gfloat toggle_pos;

  CLUTTER_ACTOR_CLASS (mx_toggle_parent_class)->allocate (actor, box, flags);

  clutter_actor_get_preferred_width (priv->handle, -1, NULL, &handle_w);
  clutter_actor_get_preferred_height (priv->handle, -1, NULL, &handle_h);

  mx_widget_get_available_area (MX_WIDGET (actor), box, &child_box);

  toggle_pos = child_box.x2 - handle_w - child_box.x1;
  priv->slide_length = toggle_pos;

  toggle_pos = toggle_pos * priv->position;


  handle_box.x1 = (gint) (child_box.x1 + toggle_pos);
  handle_box.y1 = child_box.y1;
  handle_box.x2 = handle_box.x1 + handle_w;
  handle_box.y2 = handle_box.y1 + handle_h;

  clutter_actor_allocate (priv->handle, &handle_box, flags);
}

static gboolean
mx_toggle_button_release_event (ClutterActor       *actor,
                                ClutterButtonEvent *event)
{
  MxToggle *toggle = MX_TOGGLE (actor);

  mx_toggle_set_active (toggle, !toggle->priv->active);

  return TRUE;
}

static gboolean
mx_toggle_handle_button_press_event (ClutterActor       *actor,
                                     ClutterButtonEvent *event,
                                     MxToggle           *toggle)
{
  clutter_grab_pointer (actor);

  toggle->priv->drag_offset = event->x;

  return TRUE;
}

static gboolean
mx_toggle_handle_button_release_event (ClutterActor       *actor,
                                       ClutterButtonEvent *event,
                                       MxToggle           *toggle)
{
  if (toggle->priv->last_move == 0)
    mx_toggle_set_active (toggle, !toggle->priv->active);
  else
    mx_toggle_set_active (toggle, (toggle->priv->last_move > 0.0));

  toggle->priv->drag_offset = -1;

  toggle->priv->last_move = 0;

  clutter_ungrab_pointer ();

  return TRUE;
}

static gboolean
mx_toggle_handle_motion_event (ClutterActor       *actor,
                               ClutterMotionEvent *event,
                               MxToggle           *toggle)
{
  MxTogglePrivate *priv = toggle->priv;

  if (priv->drag_offset > -1)
    {
      if (priv->slide_length)
        {
          gfloat pos;

          if (priv->active)
            pos = 1 - ((priv->drag_offset - event->x) / priv->slide_length);
          else
            pos = (event->x - priv->drag_offset) / priv->slide_length;

          if (pos - priv->position)
            priv->last_move = (pos - priv->position);

          priv->position = CLAMP (pos, 0, 1);
        }
      else
        priv->position = 0;

      clutter_actor_queue_relayout (actor);
    }

  return TRUE;
}


static void
mx_toggle_class_init (MxToggleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (MxTogglePrivate));

  object_class->get_property = mx_toggle_get_property;
  object_class->set_property = mx_toggle_set_property;
  object_class->dispose = mx_toggle_dispose;
  object_class->finalize = mx_toggle_finalize;

  actor_class->pick = mx_toggle_pick;
  actor_class->paint = mx_toggle_paint;
  actor_class->map = mx_toggle_map;
  actor_class->unmap = mx_toggle_unmap;
  actor_class->get_preferred_width = mx_toggle_background_get_preferred_width;
  actor_class->get_preferred_height = mx_toggle_background_get_preferred_height;
  actor_class->allocate = mx_toggle_allocate;
  actor_class->button_release_event = mx_toggle_button_release_event;

  pspec = g_param_spec_boolean ("active",
                                "Active",
                                "Whether the toggle switch is activated",
                                FALSE,
                                MX_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ACTIVE, pspec);
}

static void
mx_toggle_update_position (ClutterTimeline *timeline,
                           gint             msecs,
                           MxToggle        *toggle)
{
  MxTogglePrivate *priv = toggle->priv;

  priv->position = clutter_alpha_get_alpha (priv->alpha);

  clutter_actor_queue_relayout (CLUTTER_ACTOR (toggle));
}

static void
mx_toggle_init (MxToggle *self)
{
  ClutterTimeline *timeline;

  self->priv = TOGGLE_PRIVATE (self);

  self->priv->handle = g_object_new (MX_TYPE_TOGGLE_HANDLE,
                                     "reactive", TRUE, NULL);
  clutter_actor_set_parent (self->priv->handle, CLUTTER_ACTOR (self));

  timeline = clutter_timeline_new (300);
  g_signal_connect (timeline, "new-frame",
                    G_CALLBACK (mx_toggle_update_position), self);

  self->priv->alpha = clutter_alpha_new_full (timeline,
                                              CLUTTER_EASE_IN_OUT_CUBIC);

  clutter_actor_set_reactive (CLUTTER_ACTOR (self), TRUE);
  clutter_actor_set_reactive (CLUTTER_ACTOR (self->priv->handle), TRUE);

  self->priv->drag_offset = -1;
  g_signal_connect (self->priv->handle, "button-press-event",
                    G_CALLBACK (mx_toggle_handle_button_press_event), self);
  g_signal_connect (self->priv->handle, "button-release-event",
                    G_CALLBACK (mx_toggle_handle_button_release_event), self);
  g_signal_connect (self->priv->handle, "motion-event",
                    G_CALLBACK (mx_toggle_handle_motion_event), self);

}

ClutterActor *
mx_toggle_new (void)
{
  return g_object_new (MX_TYPE_TOGGLE, NULL);
}

void
mx_toggle_set_active (MxToggle *toggle,
                      gboolean  active)
{
  MxTogglePrivate *priv;

  g_return_if_fail (MX_IS_TOGGLE (toggle));

  priv = toggle->priv;

  if (priv->active != active
      || (priv->position > 0 && priv->position < 1))
    {
      ClutterTimeline *timeline;

      timeline = clutter_alpha_get_timeline (priv->alpha);

      if (clutter_timeline_is_playing (timeline))
        return;

      if (active)
        clutter_timeline_set_direction (timeline, CLUTTER_TIMELINE_FORWARD);
      else
        clutter_timeline_set_direction (timeline, CLUTTER_TIMELINE_BACKWARD);

      clutter_timeline_rewind (timeline);

      if (priv->drag_offset > -1)
        {
          clutter_alpha_set_mode (priv->alpha, CLUTTER_LINEAR);
          clutter_timeline_advance (timeline, priv->position * 300);
        }
      else
        {
          clutter_alpha_set_mode (priv->alpha, CLUTTER_EASE_IN_OUT_CUBIC);
        }

      clutter_timeline_start (timeline);

      priv->active = active;

      g_object_notify (G_OBJECT (toggle), "active");
    }
}

gboolean
mx_toggle_get_active (MxToggle *toggle)
{
  g_return_val_if_fail (MX_IS_TOGGLE (toggle), FALSE);

  return toggle->priv->active;
}