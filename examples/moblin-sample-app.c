#include <nbtk/nbtk.h>

int
main (int argc, char** argv)
{
  ClutterActor *stage, *text;
  NbtkWidget *w, *table, *button, *checkbox, *expander, *scrollview, *entry;

  clutter_init (&argc, &argv);

  nbtk_style_load_from_file (nbtk_style_get_default (), "custom.css", NULL);

  stage = clutter_stage_get_default ();
  clutter_actor_set_size (stage, 320, 240);

  table = nbtk_table_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), CLUTTER_ACTOR (table));
  clutter_actor_set_position (CLUTTER_ACTOR (table), 0, 0);
  clutter_actor_set_size (CLUTTER_ACTOR (table), 320, 240);

  button = nbtk_button_new_with_label ("Back");
  nbtk_table_add_actor_with_properties (NBTK_TABLE (table), (ClutterActor*) button, 0, 0,
                                        "x-expand", FALSE, "y-expand", FALSE, NULL);

  button = nbtk_button_new_with_label ("Forward");
  nbtk_table_add_actor_with_properties (NBTK_TABLE (table), (ClutterActor*) button, 0, 1,
                                        "x-expand", FALSE,  "y-expand", FALSE, NULL);

  entry = nbtk_entry_new ("");
  nbtk_table_add_actor_with_properties (NBTK_TABLE (table), (ClutterActor*) entry, 0, 2,
                                        "y-expand", FALSE, NULL);


  button = nbtk_button_new_with_label ("Preferences");
  nbtk_table_add_actor_with_properties (NBTK_TABLE (table), (ClutterActor*) button, 0, 3,
                                        "x-expand", FALSE,  "y-expand", FALSE, NULL);

  button = nbtk_button_new_with_label ("Close");
  clutter_actor_set_name (button, "close-button");
  nbtk_table_add_actor_with_properties (NBTK_TABLE (table), (ClutterActor*) button, 0, 4,
                                        "x-expand", FALSE,  "y-expand", FALSE, NULL);

  scrollview = nbtk_scroll_view_new ();
  nbtk_table_add_actor_with_properties (NBTK_TABLE (table), (ClutterActor*) scrollview, 1, 0,
                                        "col-span", 5, "x-expand", FALSE, NULL);

  text = clutter_text_new ();
  clutter_text_set_line_wrap (text, TRUE);
  clutter_actor_set_width (text, 640);
  clutter_text_set_text (text, "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.");

  w = nbtk_viewport_new ();
  clutter_container_add_actor (CLUTTER_CONTAINER (w),text);
  clutter_container_add_actor (CLUTTER_CONTAINER (scrollview), CLUTTER_ACTOR (w));

  clutter_actor_show (stage);

  clutter_main ();

  return 0;
}