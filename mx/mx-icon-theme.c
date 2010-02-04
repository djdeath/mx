/*
 * mx-icon-theme.c: A freedesktop icon theme object
 *
 * Copyright 2010 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Author: Chris Lord <chris@linux.intel.com>
 *
 */

#include <string.h>
#include <gio/gio.h>
#include "mx-icon-theme.h"
#include "mx-marshal.h"
#include "mx-texture-cache.h"

G_DEFINE_TYPE (MxIconTheme, mx_icon_theme, G_TYPE_OBJECT)

enum
{
  CHANGED,

  LAST_SIGNAL
};

#define ICON_THEME_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MX_TYPE_ICON_THEME, MxIconThemePrivate))

typedef enum
{
  MX_FIXED,
  MX_SCALABLE,
  MX_THRESHOLD
} MxIconType;

typedef struct
{
  gint         size;
  gchar       *path;
  MxIconType   type;
  gint         min_size;
  gint         max_size;
  gint         threshold;
} MxIconData;

struct _MxIconThemePrivate
{
  GList      *search_paths;
  GHashTable *icon_hash;

  gchar      *theme;
  GKeyFile   *theme_file;
  GList      *theme_fallbacks;

  GKeyFile   *hicolor_file;
};

static guint signals[LAST_SIGNAL] = { 0, };


static void
mx_icon_theme_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
mx_icon_theme_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
mx_icon_theme_dispose (GObject *object)
{
  G_OBJECT_CLASS (mx_icon_theme_parent_class)->dispose (object);
}

static void
mx_icon_theme_finalize (GObject *object)
{
  MxIconTheme *self = MX_ICON_THEME (object);
  MxIconThemePrivate *priv = self->priv;

  mx_icon_theme_set_search_paths (self, NULL);
  g_hash_table_unref (priv->icon_hash);
  g_free (priv->theme);

  if (priv->theme_file)
    g_key_file_free (priv->theme_file);

  while (priv->theme_fallbacks)
    {
      g_key_file_free ((GKeyFile *)priv->theme_fallbacks->data);
      priv->theme_fallbacks = g_list_delete_link (priv->theme_fallbacks,
                                                  priv->theme_fallbacks);
    }

  if (priv->hicolor_file)
    g_key_file_free (priv->hicolor_file);

  G_OBJECT_CLASS (mx_icon_theme_parent_class)->finalize (object);
}

static void
mx_icon_theme_class_init (MxIconThemeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MxIconThemePrivate));

  object_class->get_property = mx_icon_theme_get_property;
  object_class->set_property = mx_icon_theme_set_property;
  object_class->dispose = mx_icon_theme_dispose;
  object_class->finalize = mx_icon_theme_finalize;

  signals[CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (MxIconThemeClass, changed),
                  NULL, NULL,
                  _mx_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}

static GKeyFile *
mx_icon_theme_load_theme (MxIconTheme *self, const gchar *name)
{
  GList *p;
  GKeyFile *key_file;
  MxIconThemePrivate *priv = self->priv;

  key_file = g_key_file_new ();
  for (p = priv->search_paths; p; p = p->next)
    {
      gboolean success;

      const gchar *path = p->data;
      gchar *key_path = g_build_filename (path, name, "index.theme", NULL);

      success = g_key_file_load_from_file (key_file, key_path, 0, NULL);
      g_free (key_path);

      if (success)
        return key_file;
    }

  g_key_file_free (key_file);

  return NULL;
}

static void
mx_icon_theme_icon_data_free (MxIconData *data)
{
  g_free (data->path);
  g_free (data);
}

static void
mx_icon_theme_icon_hash_free (gpointer data)
{
  GList *icon_list = (GList *)data;
  while (icon_list)
    {
      mx_icon_theme_icon_data_free ((MxIconData *)icon_list->data);
      icon_list = g_list_delete_link (icon_list, icon_list);
    }
}

static MxIconData *
mx_icon_theme_icon_data_new (gint         size,
                             const gchar *path,
                             MxIconType   type,
                             gint         min_size,
                             gint         max_size,
                             gint         threshold)
{
  MxIconData *data = g_new (MxIconData, 1);
  data->size = size;
  data->path = g_strdup (path);
  data->type = type;
  data->min_size = min_size;
  data->max_size = max_size;
  data->threshold = threshold;
  return data;
}

static gboolean
mx_icon_theme_equal_func (GThemedIcon *icon,
                          GThemedIcon *search)
{
  gint i;

  const gchar * const *names1 = g_themed_icon_get_names (icon);
  const gchar * const *names2 = g_themed_icon_get_names (search);

  for (i = 0; names1[i] && names2[i]; i++)
    if (!g_str_equal (names1[i], names2[i]))
      return FALSE;

  return TRUE;
}

static guint
mx_icon_theme_hash (GThemedIcon *icon)
{
  const gchar * const *names = g_themed_icon_get_names (icon);
  return g_str_hash (names[0]);
}

static void
mx_icon_theme_init (MxIconTheme *self)
{
  gchar *path;
  const gchar *theme;
  MxIconThemePrivate *priv = self->priv = ICON_THEME_PRIVATE (self);

  /* These three paths are named in the icon theme spec */
  path = g_build_filename (G_DIR_SEPARATOR_S, "usr", "share", "pixmaps", NULL);
  priv->search_paths = g_list_prepend (priv->search_paths, path);

  path = g_build_filename (G_DIR_SEPARATOR_S, "usr", "share", "icons", NULL);
  priv->search_paths = g_list_prepend (priv->search_paths, path);

  path = g_build_filename (g_get_home_dir (), ".icons", NULL);
  priv->search_paths = g_list_prepend (priv->search_paths, path);

  priv->icon_hash = g_hash_table_new_full ((GHashFunc)mx_icon_theme_hash,
                                           (GEqualFunc)mx_icon_theme_equal_func,
                                           g_object_unref,
                                           mx_icon_theme_icon_hash_free);

  priv->hicolor_file = mx_icon_theme_load_theme (self, "hicolor");

  theme = g_getenv ("MX_ICON_THEME");
  if (!theme)
    theme = "moblin";

  mx_icon_theme_set_theme (self, theme);
}

MxIconTheme *
mx_icon_theme_new (void)
{
  return g_object_new (MX_TYPE_ICON_THEME, NULL);
}

/**
 * mx_icon_theme_get_default:
 *
 * Return the default #MxIconTheme object used by the toolkit.
 *
 * Return value: (transfer none): an #MxIconTheme.
 */
MxIconTheme *
mx_icon_theme_get_default (void)
{
  static MxIconTheme *default_icon_theme = NULL;

  if (!default_icon_theme)
    default_icon_theme = mx_icon_theme_new ();

  return default_icon_theme;
}

const gchar *
mx_icon_theme_get_theme (MxIconTheme *theme)
{
  g_return_val_if_fail (MX_IS_ICON_THEME (theme), NULL);

  return theme->priv->theme;
}

void
mx_icon_theme_set_theme (MxIconTheme *theme,
                         const gchar *theme_name)
{
  gchar *fallbacks;
  MxIconThemePrivate *priv;

  g_return_if_fail (MX_IS_ICON_THEME (theme));
  g_return_if_fail (theme_name != NULL);

  priv = theme->priv;

  if (g_str_equal (theme_name, "hicolor"))
    return;

  if (priv->theme && g_str_equal (priv->theme, theme_name))
    return;

  /* Clear old data */
  g_hash_table_remove_all (priv->icon_hash);

  g_free (priv->theme);

  if (priv->theme_file)
    g_key_file_free (priv->theme_file);

  while (priv->theme_fallbacks)
    {
      g_key_file_free ((GKeyFile *)priv->theme_fallbacks->data);
      priv->theme_fallbacks = g_list_delete_link (priv->theme_fallbacks,
                                                  priv->theme_fallbacks);
    }

  /* Load new theme file */
  priv->theme = g_strdup (theme_name);
  priv->theme_file = mx_icon_theme_load_theme (theme, theme_name);

  if (!priv->theme_file)
    {
      g_warning ("Error loading theme");
      return;
    }

  /* Load fallbacks */
  fallbacks = g_key_file_get_string (priv->theme_file,
                                     "Icon Theme",
                                     "Inherits",
                                     NULL);
  if (fallbacks)
    {
      gint i = 0;
      gint fallbacks_len = strlen (fallbacks);
      while (i < fallbacks_len)
        {
          GKeyFile *theme_file;

          gchar *fallback = fallbacks + i;
          i += strlen (fallback) + 1;

          /* Skip hicolor, we keep it loaded all the time */
          if (g_str_equal (fallback, "hicolor"))
            continue;

          theme_file = mx_icon_theme_load_theme (theme, fallback);
          if (theme_file)
            priv->theme_fallbacks = g_list_append (priv->theme_fallbacks,
                                                   theme_file);
        }
      g_free (fallbacks);
    }
}

static GList *
mx_icon_theme_theme_load_icon (MxIconTheme *self,
                               GKeyFile    *theme_file,
                               const gchar *icon,
                               GIcon       *store_icon)
{
  gchar *dirs;
  gchar *theme_name;

  GList *data = NULL;
  MxIconThemePrivate *priv = self->priv;

  theme_name = g_key_file_get_string (theme_file,
                                      "Icon Theme",
                                      "Name",
                                      NULL);
  if (!theme_name)
    return NULL;

  dirs = g_key_file_get_string (theme_file,
                                "Icon Theme",
                                "Directories",
                                NULL);

  if (dirs)
    {
      gint i;
      gint dirs_len = strlen (dirs);

      g_strdelimit (dirs, ",", '\0');

      i = 0;
      while (i < dirs_len)
        {
          GList *p;
          MxIconType type;
          gchar *type_string;
          gint size, min, max, threshold;

          const gchar *dir = dirs + i;
          i += strlen (dir) + 1;

          size = g_key_file_get_integer (theme_file,
                                         dir,
                                         "Size",
                                         NULL);
          if (!size)
            continue;

          type_string = g_key_file_get_string (theme_file,
                                               dir,
                                               "Type",
                                               NULL);

          type = MX_FIXED;
          min = max = threshold = 0;
          if (type_string)
            {
              if (g_str_equal (type_string, "Scalable"))
                {
                  type = MX_SCALABLE;
                  min = g_key_file_get_integer (theme_file,
                                                dir,
                                                "MinSize",
                                                NULL);
                  if (!min)
                    min = size;

                  max = g_key_file_get_integer (theme_file,
                                                dir,
                                                "MaxSize",
                                                NULL);
                  if (!max)
                    max = size;
                }
              else if (g_str_equal (type_string, "Threshold"))
                {
                  type = MX_THRESHOLD;
                  threshold = g_key_file_get_integer (theme_file,
                                                      dir,
                                                      "Threshold",
                                                      NULL);
                  if (!threshold)
                    threshold = 2;

                  min = size - threshold;
                  max = size + threshold;
                }
              g_free (type_string);
            }

          for (p = priv->search_paths; p; p = p->next)
            {
              gchar *file;

              MxIconData *icon_data = NULL;
              const gchar *search_path = p->data;
              gchar *path = g_build_filename (search_path,
                                              theme_name,
                                              dir,
                                              NULL);

              /* Try png first, then svg and xpm */
              file = g_strconcat (path, G_DIR_SEPARATOR_S, icon, ".png", NULL);
              if (!g_file_test (file, G_FILE_TEST_EXISTS |
                                      G_FILE_TEST_IS_REGULAR))
                {
                  g_free (file);
                  file = g_strconcat (path, G_DIR_SEPARATOR_S, icon, ".svg",
                                      NULL);
                  if (!g_file_test (file, G_FILE_TEST_EXISTS |
                                          G_FILE_TEST_IS_REGULAR))
                    {
                      g_free (file);
                      file = g_strconcat (path, G_DIR_SEPARATOR_S, icon, ".xpm",
                                          NULL);
                      if (!g_file_test (file, G_FILE_TEST_EXISTS |
                                              G_FILE_TEST_IS_REGULAR))
                        {
                          g_free (file);
                          file = NULL;
                        }
                    }
                }

              if (file)
                {
                  icon_data = mx_icon_theme_icon_data_new (size,
                                                           file,
                                                           type,
                                                           min,
                                                           max,
                                                           threshold);
                  g_free (file);

                  data = g_list_prepend (data, icon_data);
                }
            }
        }
      g_free (dirs);
    }

  if (data)
    {
      data = g_list_reverse (data);
      if (!store_icon)
        store_icon = g_themed_icon_new_with_default_fallbacks (icon);
      else
        store_icon = g_object_ref (store_icon);

      g_hash_table_insert (priv->icon_hash, store_icon, data);
    }
  g_free (theme_name);

  return data;
}

static GList *
mx_icon_theme_load_icon (MxIconTheme *theme,
                         const gchar *icon_name,
                         GIcon       *store_icon)
{
  GList *data, *f;
  MxIconThemePrivate *priv = theme->priv;

  /* Try the set theme */
  if (priv->theme_file &&
      (data = mx_icon_theme_theme_load_icon (theme, priv->theme_file,
                                             icon_name, store_icon)))
    return data;

  /* Try the inherited themes */
  for (f = priv->theme_fallbacks; f; f = f->next)
    {
      GKeyFile *theme_file = f->data;
      if ((data = mx_icon_theme_theme_load_icon (theme, theme_file, icon_name,
                                                 store_icon)))
        return data;
    }

  /* Try the hicolor theme */
  if (priv->hicolor_file &&
      (data = mx_icon_theme_theme_load_icon (theme, priv->hicolor_file,
                                             icon_name, store_icon)))
    return data;

  return NULL;

}

static GList *
mx_icon_theme_copy_data_list (GList *data)
{
  GList *d, *new_data = g_list_copy (data);
  for (d = new_data; d; d = d->next)
    {
      MxIconData *icon_data;

      d->data = g_memdup (d->data, sizeof (MxIconData));
      icon_data = d->data;
      icon_data->path = g_strdup (icon_data->path);
    }
  return new_data;
}

static GList *
mx_icon_theme_get_icons (MxIconTheme *theme,
                         const gchar *icon_name)
{
  gint i;
  GIcon *icon;
  GList *data;

  const gchar * const *names = NULL;
  MxIconThemePrivate *priv = theme->priv;

  /* Load the icon, or a fallback */
  icon = g_themed_icon_new_with_default_fallbacks (icon_name);
  names = g_themed_icon_get_names (G_THEMED_ICON (icon));
  if (!names)
    {
      g_object_unref (icon);
      return NULL;
    }

  data = NULL;
  for (i = 0; names[i]; i++)
    {
      /* See if we've loaded this before */
      GIcon *single_icon = g_themed_icon_new (names[i]);
      data = g_hash_table_lookup (priv->icon_hash, single_icon);
      g_object_unref (single_icon);

      /* Found in cache on first hit, break */
      if (data && (i == 0))
        break;

      /* Found in cache after searching the disk, store again as a new icon */
      if (data)
        {
          /* If we found this as a fallback, store it so we don't look on
           * disk again.
           */
          data = mx_icon_theme_copy_data_list (data);
          g_hash_table_insert (priv->icon_hash, g_object_ref (icon), data);

          break;
        }

      /* Try to load from disk */
      if ((data = mx_icon_theme_load_icon (theme, names[i], icon)))
        break;
    }

  g_object_unref (icon);

  return data;
}

static MxIconData *
mx_icon_theme_lookup_internal (MxIconTheme *theme,
                               const gchar *icon_name,
                               gint         size)
{
  MxIconData *best_match;
  GList *d, *data;
  gint distance;

  data = mx_icon_theme_get_icons (theme, icon_name);
  if (!data)
    return NULL;

  best_match = NULL;
  distance = G_MAXINT;
  for (d = data; d; d = d->next)
    {
      gint current_distance;
      MxIconData *current = d->data;

      switch (current->type)
        {
        case MX_FIXED:
          current_distance = ABS (size - current->size);
          break;

        case MX_SCALABLE:
        case MX_THRESHOLD:
          if (size < current->min_size)
            current_distance = current->min_size - size;
          else if (size > current->max_size)
            current_distance = size - current->max_size;
          else
            current_distance = 0;
          break;

        default:
          g_warning ("Unknown icon type in cache");
          current_distance = G_MAXINT - 1;
          break;
        }

      if (current_distance < distance)
        {
          distance = current_distance;
          best_match = current;
        }
    }

  if (!best_match)
    {
      g_warning ("No match found, but icon is in cache");
      return NULL;
    }

  return best_match;
}

/**
 * mx_icon_theme_lookup:
 * @theme: an #MxIconTheme
 * @icon_name: The name of the icon
 * @size: The desired size of the icon
 *
 * If the icon is available, returns a #CoglHandle of the icon.
 *
 * Return value: a #CoglHandle of the icon, or %NULL.
 */
CoglHandle
mx_icon_theme_lookup (MxIconTheme *theme,
                      const gchar *icon_name,
                      gint         size)
{
  MxTextureCache *texture_cache;
  MxIconData *icon_data;

  g_return_val_if_fail (MX_IS_ICON_THEME (theme), NULL);
  g_return_val_if_fail (icon_name, NULL);
  g_return_val_if_fail (size > 0, NULL);

  if (!(icon_data = mx_icon_theme_lookup_internal (theme, icon_name, size)))
    return NULL;

  texture_cache = mx_texture_cache_get_default ();
  return mx_texture_cache_get_cogl_texture (texture_cache, icon_data->path);
}

/**
 * mx_icon_theme_lookup_texture:
 * @theme: an #MxIconTheme
 * @icon_name: The name of the icon
 * @size: The desired size of the icon
 *
 * If the icon is available, returns a #ClutterTexture of the icon.
 *
 * Return value: (transfer none): a #ClutterTexture of the icon, or %NULL.
 */
ClutterTexture *
mx_icon_theme_lookup_texture (MxIconTheme *theme,
                              const gchar *icon_name,
                              gint         size)
{
  MxTextureCache *texture_cache;
  MxIconData *icon_data;

  g_return_val_if_fail (MX_IS_ICON_THEME (theme), NULL);
  g_return_val_if_fail (icon_name, NULL);
  g_return_val_if_fail (size > 0, NULL);

  if (!(icon_data = mx_icon_theme_lookup_internal (theme, icon_name, size)))
    return NULL;

  texture_cache = mx_texture_cache_get_default ();
  return mx_texture_cache_get_texture (texture_cache, icon_data->path);
}

gboolean
mx_icon_theme_has_icon (MxIconTheme *theme,
                        const gchar *icon_name)
{
  g_return_val_if_fail (MX_IS_ICON_THEME (theme), FALSE);
  g_return_val_if_fail (icon_name, FALSE);

  if (mx_icon_theme_get_icons (theme, icon_name))
    return TRUE;
  else
    return FALSE;
}

const GList *
mx_icon_theme_get_search_paths (MxIconTheme *theme)
{
  g_return_val_if_fail (MX_IS_ICON_THEME (theme), NULL);

  return theme->priv->search_paths;
}

void
mx_icon_theme_set_search_paths (MxIconTheme *theme,
                                const GList *paths)
{
  GList *p;
  MxIconThemePrivate *priv;

  g_return_if_fail (MX_IS_ICON_THEME (theme));

  priv = theme->priv;
  while (priv->search_paths)
    {
      g_free (priv->search_paths->data);
      priv->search_paths = g_list_delete_link (priv->search_paths,
                                               priv->search_paths);
    }

  priv->search_paths = g_list_copy ((GList *)paths);
  for (p = priv->search_paths; p; p = p->next)
    p->data = g_strdup ((const gchar *)p->data);
}
