#include "asf_convert_gui.h"

SIGNAL_CALLBACK void
on_add_button_clicked(GtkWidget *widget)
{
  GtkWidget *input_entry;
  G_CONST_RETURN gchar *in_data;
  gboolean result;
  
  input_entry = 
    glade_xml_get_widget(glade_xml, "input_entry");

  in_data =
    gtk_entry_get_text(GTK_ENTRY(input_entry));

  if (strlen(in_data) == 0)
  {
    message_box("Enter the name of a data file to add to the list.");
    return;
  }

  if (!g_file_test(in_data, G_FILE_TEST_EXISTS))
  {
    gchar * message =
      (gchar *) g_malloc(sizeof(gchar) * (strlen(in_data) + 256));

    g_sprintf(message, "Error: Couldn't find the file \"%s\".", in_data);
    message_box(message);
    return;
  }

  /* add file to the list */
  result = add_to_files_list(in_data);

  if (!result)
  {
      gchar * ext;
      gchar * message =
        (gchar *) g_malloc(sizeof(gchar) * (strlen(in_data) + 256));

      ext = strrchr(in_data, '.');
      if (!ext)
      {
          message_box("Error: Unknown file type, did not have extension.");
      }
      else
      {
          ++ext;
          g_sprintf(message, "Error: Unknown file type: %s!", ext);
          message_box(message);
      }
  }
}

SIGNAL_CALLBACK void
on_browse_input_files_button_clicked(GtkWidget *widget)
{
#ifdef FILE_CHOOSER_AVAILABLE
  GtkWidget *file_selection_dialog =
    glade_xml_get_widget(glade_xml, "input_file_chooser");

  GtkFileFilter *filter = gtk_file_filter_new();
  gtk_file_filter_add_pattern(filter, "*.D"); // FIXME

  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_selection_dialog), filter);
#else
  GtkWidget *file_selection_dialog =
    glade_xml_get_widget(glade_xml, "input_file_selection");
#endif

  gtk_widget_show(file_selection_dialog);
}

void
hide_input_file_selection_dialog()
{
  GtkWidget *file_selection_dialog =
    glade_xml_get_widget(glade_xml, "input_file_selection");

  gtk_widget_hide(file_selection_dialog);
}

SIGNAL_CALLBACK void
on_input_file_selection_cancel_button_clicked(GtkWidget *widget)
{
  hide_input_file_selection_dialog();
}

SIGNAL_CALLBACK gboolean
on_input_file_selection_delete_event(GtkWidget *w)
{
  hide_input_file_selection_dialog();
  return TRUE;
}

SIGNAL_CALLBACK gboolean
on_input_file_selection_destroy_event(GtkWidget *w)
{
  hide_input_file_selection_dialog();
  return TRUE;
}

SIGNAL_CALLBACK gboolean
on_input_file_selection_destroy(GtkWidget *w)
{
  hide_input_file_selection_dialog();
  return TRUE;
}

SIGNAL_CALLBACK void
on_input_file_selection_ok_button_clicked(GtkWidget *widget)
{
  GtkWidget *file_selection_dialog;
  gchar **selections;
  gchar **current;
  int i, n;
  
  file_selection_dialog =
    glade_xml_get_widget(glade_xml, "input_file_selection");

  selections = gtk_file_selection_get_selections(
             GTK_FILE_SELECTION(file_selection_dialog));

  current = selections;
  i = n = 0;
  
  while (*current)
  {
    /* second clause here allows silent fail for .L files, PR 92 */
    if (add_to_files_list(*current) || is_L_file(*current))
	++i;
            
    ++current;
    ++n;
  }

  if (i != n)
  {
    if (n == 1 || i == 0)
    {
      message_box("Error: Unrecognized extension.");
    }
    else
    {
      message_box("Some of the files were not added -- unknown extensions.");
    }
  }
  
  g_strfreev(selections);
  gtk_widget_hide(file_selection_dialog);
}

void
hide_input_file_chooser_dialog()
{
  GtkWidget *file_selection_dialog =
    glade_xml_get_widget(glade_xml, "input_file_chooser");

  gtk_widget_hide(file_selection_dialog);
}

SIGNAL_CALLBACK void
on_input_file_chooser_cancel_button_clicked(GtkWidget *widget)
{
  hide_input_file_chooser_dialog();
}

SIGNAL_CALLBACK gboolean
on_input_file_chooser_delete_event(GtkWidget *w)
{
  hide_input_file_chooser_dialog();
  return TRUE;
}

SIGNAL_CALLBACK gboolean
on_input_file_chooser_destroy_event(GtkWidget *w)
{
  hide_input_file_chooser_dialog();
  return TRUE;
}

SIGNAL_CALLBACK gboolean
on_input_file_chooser_destroy(GtkWidget *w)
{
  hide_input_file_chooser_dialog();
  return TRUE;
}

SIGNAL_CALLBACK void
on_input_file_chooser_ok_button_clicked(GtkWidget *widget)
{
  GtkWidget *file_chooser_dialog;
  GSList *selections;
  GSList *current;
  int i, n;
  
  file_chooser_dialog =
    glade_xml_get_widget(glade_xml, "input_file_chooser");

  selections =
    gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(file_chooser_dialog));

  current = selections;
  i = n = 0;
  
  while (current)
  {
    gchar * file = (gchar *) current->data;

    /* second clause here allows silent fail for .L files, PR 92 */
    if (add_to_files_list(file) || is_L_file(file))
	++i;
            
    current = g_slist_next(current);
    ++n;

    g_free(file);
  }

  if (i != n)
  {
    if (n == 1 || i == 0)
    {
      message_box("Error: Unrecognized extension.");
    }
    else
    {
      message_box("Some of the files were not added -- unknown extensions.");
    }
  }
  
  g_slist_free(selections);
  gtk_widget_hide(file_chooser_dialog);
}
