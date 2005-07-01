/* 
 * gnc-main-window.c -- GtkWindow which represents the
 *	GnuCash main window.
 *
 * Copyright (C) 2003 Jan Arne Petersen <jpetersen@uni-bonn.de>
 * Copyright (C) 2003 David Hampton <hampton@employees.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, contact:
 *
 * Free Software Foundation           Voice:  +1-617-542-5942
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652
 * Boston, MA  02111-1307,  USA       gnu@gnu.org
 */

#include "config.h"

#include <gtk/gtk.h>

#include "gnc-embedded-window.h"

#include "gnc-engine.h"
#include "gnc-engine-util.h"
#include "gnc-gnome-utils.h"
#include "gnc-dir.h"
#include "gnc-gobject-utils.h"
#include "gnc-gui-query.h"
#include "gnc-plugin.h"
#include "gnc-plugin-manager.h"
#include "gnc-session.h"
#include "gnc-ui.h"
#include "gnc-window.h"
#include "messages.h"

/** Static Globals *******************************************************/
static short module = MOD_GUI;

/** Declarations *********************************************************/
static void gnc_embedded_window_class_init (GncEmbeddedWindowClass *klass);
static void gnc_embedded_window_init (GncEmbeddedWindow *window, GncEmbeddedWindowClass *klass);
static void gnc_embedded_window_finalize (GObject *object);
static void gnc_embedded_window_dispose (GObject *object);

static void gnc_window_embedded_window_init (GncWindowIface *iface);

static void gnc_embedded_window_setup_window (GncEmbeddedWindow *window);


struct GncEmbeddedWindowPrivate
{
  GtkWidget *menu_dock;
  GtkWidget *toolbar_dock;
  GtkWidget *statusbar;

  GtkActionGroup *action_group;

  GncPluginPage *page;
  GtkWidget     *parent_window;
};

static GObjectClass *parent_class = NULL;

GType
gnc_embedded_window_get_type (void)
{
  static GType gnc_embedded_window_type = 0;

  if (gnc_embedded_window_type == 0) {
    static const GTypeInfo our_info = {
      sizeof (GncEmbeddedWindowClass),
      NULL,
      NULL,
      (GClassInitFunc) gnc_embedded_window_class_init,
      NULL,
      NULL,
      sizeof (GncEmbeddedWindow),
      0,
      (GInstanceInitFunc) gnc_embedded_window_init
    };

    static const GInterfaceInfo plugin_info = {
      (GInterfaceInitFunc) gnc_window_embedded_window_init,
      NULL,
      NULL
    };

    gnc_embedded_window_type = g_type_register_static (GTK_TYPE_VBOX,
						       "GncEmbeddedWindow",
						       &our_info, 0);
    g_type_add_interface_static (gnc_embedded_window_type,
				 GNC_TYPE_WINDOW,
				 &plugin_info);
  }

  return gnc_embedded_window_type;
}

void
gnc_embedded_window_open_page (GncEmbeddedWindow *window,
			       GncPluginPage *page)
{
  ENTER("window %p, page %p", window, page);
  g_return_if_fail (GNC_IS_EMBEDDED_WINDOW (window));
  g_return_if_fail (GNC_IS_PLUGIN_PAGE (page));
  g_return_if_fail (window->priv->page == NULL);

  window->priv->page = page;
  page->window = GTK_WIDGET(window);
  page->notebook_page = gnc_plugin_page_create_widget (page);
  g_object_set_data (G_OBJECT (page->notebook_page), PLUGIN_PAGE_LABEL, page);

  gtk_box_pack_end(GTK_BOX(window), page->notebook_page, TRUE, TRUE, 2);
  gnc_plugin_page_inserted (page);

  gnc_plugin_page_merge_actions (page, window->ui_merge);
  LEAVE(" ");
}

void
gnc_embedded_window_close_page (GncEmbeddedWindow *window,
				GncPluginPage *page)
{
  ENTER("window %p, page %p", window, page);
  g_return_if_fail (GNC_IS_EMBEDDED_WINDOW (window));
  g_return_if_fail (GNC_IS_PLUGIN_PAGE (page));

  if (!page->notebook_page) {
    LEAVE("no displayed widget");
    return;
  }

  gtk_container_remove (GTK_CONTAINER(window), GTK_WIDGET(window->priv->page));
  window->priv->page = NULL;
  gnc_plugin_page_removed (page);

  gnc_plugin_page_unmerge_actions (page, window->ui_merge);
  gtk_ui_manager_ensure_update (window->ui_merge);

  gnc_plugin_page_destroy_widget (page);
  g_object_unref(page);
  LEAVE(" ");
}

GncPluginPage *
gnc_embedded_window_get_page (GncEmbeddedWindow *window)
{
  return window->priv->page;
}


static void
gnc_embedded_window_class_init (GncEmbeddedWindowClass *klass)
{
  GObjectClass *object_class;
  ENTER("klass %p", klass);
  object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gnc_embedded_window_finalize;
  object_class->dispose = gnc_embedded_window_dispose;
  LEAVE(" ");
}

static void
gnc_embedded_window_init (GncEmbeddedWindow *window,
			  GncEmbeddedWindowClass *klass)
{
  ENTER("window %p", window);
  window->priv = g_new0 (GncEmbeddedWindowPrivate, 1);

  gnc_embedded_window_setup_window (window);

  gnc_gobject_tracking_remember(G_OBJECT(window),
				G_OBJECT_CLASS(klass));
  LEAVE(" ");
}

static void
gnc_embedded_window_finalize (GObject *object)
{
  GncEmbeddedWindow *window;

  ENTER("object %p", object);
  g_return_if_fail (object != NULL);
  g_return_if_fail (GNC_IS_EMBEDDED_WINDOW (object));

  window = GNC_EMBEDDED_WINDOW (object);

  g_return_if_fail (window->priv != NULL);

  g_free (window->priv);

  gnc_gobject_tracking_forget(object);
  G_OBJECT_CLASS (parent_class)->finalize (object);
  LEAVE(" ");
}

static void
gnc_embedded_window_dispose (GObject *object)
{
  GncEmbeddedWindow *window;

  ENTER("object %p", object);
  g_return_if_fail (object != NULL);
  g_return_if_fail (GNC_IS_EMBEDDED_WINDOW (object));

  window = GNC_EMBEDDED_WINDOW (object);
  if (window->priv->page) {
    DEBUG("unreffing page %p (count currently %d)", window->priv->page,
	  G_OBJECT(window->priv->page)->ref_count);
    g_object_unref(window->priv->page);
    window->priv->page = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
  LEAVE(" ");
}

static void
gnc_embedded_window_add_widget (GtkUIManager *merge,
				GtkWidget *widget,
				GncEmbeddedWindow *window)
{
  ENTER("merge %p, new widget %p, window %p", merge, widget, window);
  if (GTK_IS_TOOLBAR (widget)) {
    window->priv->toolbar_dock = widget;
  }

  gtk_box_pack_start (GTK_BOX (window->priv->menu_dock), widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);
  LEAVE(" ");
}

static void
gnc_embedded_window_setup_window (GncEmbeddedWindow *window)
{
  GncEmbeddedWindowPrivate *priv;

  ENTER("window %p", window);
  priv = window->priv;

  /* Create widgets and add them to the window */
  gtk_widget_show (GTK_WIDGET(window));

  priv->menu_dock = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (priv->menu_dock);
  gtk_box_pack_start (GTK_BOX (window), priv->menu_dock, TRUE, TRUE, 0);

  priv->statusbar = gtk_statusbar_new ();
  gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR(priv->statusbar), FALSE);
  gtk_widget_show (priv->statusbar);
  gtk_box_pack_end (GTK_BOX (window), priv->statusbar, FALSE, TRUE, 0);

  window->ui_merge = gtk_ui_manager_new ();
  g_signal_connect (G_OBJECT (window->ui_merge), "add_widget",
		    G_CALLBACK (gnc_embedded_window_add_widget), window);

  priv->action_group = NULL;
  LEAVE(" ");
}

GncEmbeddedWindow *
gnc_embedded_window_new (const gchar *action_group_name,
			 GtkActionEntry *action_entries,
			 gint n_action_entries,
			 const gchar *ui_filename,
			 GtkWidget *enclosing_win,
			 gboolean add_accelerators,
			 gpointer user_data)
{
  GncEmbeddedWindowPrivate *priv;
  GncEmbeddedWindow *window;
  gchar *ui_fullname;
  GError *error = NULL;
  guint merge_id;

  ENTER("group %s, first %p, num %d, ui file %s, parent %p, add accelerators %d, user data %p",
	action_group_name, action_entries, n_action_entries, ui_filename,
	enclosing_win, add_accelerators, user_data);
  window = g_object_new (GNC_TYPE_EMBEDDED_WINDOW, NULL);
  priv = window->priv;

  /* Determine the full pathname of the ui file */
  ui_fullname = gnc_gnome_locate_ui_file(ui_filename);

  /* Create menu and toolbar information */
  priv->action_group = gtk_action_group_new (action_group_name);
  gtk_action_group_add_actions (priv->action_group, action_entries,
				n_action_entries, user_data);
  gtk_ui_manager_insert_action_group (window->ui_merge, priv->action_group, 0);
  merge_id = gtk_ui_manager_add_ui_from_file (window->ui_merge, ui_fullname,
					      &error);

  /* Error checking */
  g_assert(merge_id || error);
  if (error) {
    g_critical("Failed to load ui file.\n  Filename %s\n  Error %s",
	       ui_fullname, error->message);
    g_error_free(error);
    g_free(ui_fullname);
    LEAVE("window %p", window);
    return window;
  }

  /* Add accelerators (if wanted) */
  if (add_accelerators)
    gtk_window_add_accel_group (GTK_WINDOW(enclosing_win),
				gtk_ui_manager_get_accel_group(window->ui_merge));

  gtk_ui_manager_ensure_update (window->ui_merge);
  g_free(ui_fullname);
  LEAVE("window %p", window);
  return window;
}

static GtkWidget *
gnc_embedded_window_get_statusbar (GncWindow *window_in)
{
  GncEmbeddedWindowPrivate *priv;
  GncEmbeddedWindow *window;

  g_return_val_if_fail (GNC_IS_EMBEDDED_WINDOW (window_in), NULL);

  window = GNC_EMBEDDED_WINDOW(window_in);
  priv = window->priv;
  return priv->statusbar;
}

static void
gnc_window_embedded_window_init (GncWindowIface *iface)
{
	iface->get_statusbar = gnc_embedded_window_get_statusbar;
}
