/*
 * mlterm-menu - popup menu style configration tool for mlterm
 * 
 * $Id$
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <gtk/gtk.h>

#include <kiklib/kik_debug.h>
#include <kiklib/kik_conf_io.h>

#ifndef SYSCONFDIR
#  define CONFIG_PATH "/etc"
#else
#  define CONFIG_PATH SYSCONFDIR
#endif

#define MENU_RCFILE "mlterm/menu"

static GScannerConfig menu_scanner_config = {
    " \t\n",
    G_CSET_A_2_Z G_CSET_a_2_z "-_",
    G_CSET_A_2_Z G_CSET_a_2_z "-_",
    "#\n",
    TRUE,
    FALSE,
    TRUE,
    FALSE,
    TRUE,
    TRUE,
    FALSE,
    TRUE,
    FALSE,
    TRUE,
    TRUE,
    TRUE,
    FALSE,
    TRUE,
    TRUE,
    TRUE,
    FALSE,
    FALSE,
    TRUE,
    FALSE,
    FALSE,
};

static char* progname;

GtkWidget* create_menu();

int append_menu_from_file(GtkMenu* menu, char* filename);
int append_menu_from_scanner(GtkMenu* menu, GScanner* scanner, int level);
int append_pty_list(GtkMenu* menu);

void activate_callback(GtkWidget* widget, gpointer data);
char* get_value(char* key);

int main(int argc, char* argv[])
{
    GtkWidget* menu;

    gtk_set_locale();

    progname = argv[0];
    gtk_init(&argc, &argv);
    kik_set_sys_conf_dir(CONFIG_PATH);

    menu = create_menu();

    gtk_signal_connect(GTK_OBJECT(menu), "deactivate",
                       gtk_main_quit, NULL);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, 0);

    gtk_main();

    return 0;
}


GtkWidget* create_menu()
{
    GtkWidget* menu;
    char* rc_path;

    menu = gtk_menu_new();

    if ((rc_path = kik_get_sys_rc_path(MENU_RCFILE))) {
        append_menu_from_file(GTK_MENU(menu), rc_path);
        free(rc_path);
    }

    if ((rc_path = kik_get_user_rc_path(MENU_RCFILE))) {
        append_menu_from_file(GTK_MENU(menu), rc_path);
        free(rc_path);
    }

    gtk_widget_show_all(menu);

    return menu;
}


int append_menu_from_file(GtkMenu* menu, char* filename)
{
    FILE* file;
    GScanner* scanner;

    file = fopen(filename, "r");
    if (file == NULL) {
        return 0;
    }

    scanner = g_scanner_new(&menu_scanner_config);
    g_scanner_input_file(scanner, fileno(file));

    append_menu_from_scanner(menu, scanner, 0);

    g_scanner_destroy(scanner);

    fclose(file);

    return 1;
}

int append_menu_from_scanner(GtkMenu* menu, GScanner* scanner, int level)
{
    GtkWidget* item;
    GtkWidget* submenu;
    GTokenType tt;
    GTokenValue tv;

    while (g_scanner_peek_next_token(scanner) != G_TOKEN_EOF) {
        tt = g_scanner_get_next_token(scanner);

        switch (tt) {
        case G_TOKEN_STRING:
            tv = g_scanner_cur_value(scanner);
            item = gtk_menu_item_new_with_label(tv.v_string);
            gtk_menu_append(menu, item);

            switch (g_scanner_peek_next_token(scanner)) {
            case G_TOKEN_STRING:
                g_scanner_get_next_token(scanner);
                tv = g_scanner_cur_value(scanner);

                gtk_signal_connect(GTK_OBJECT(item), "activate",
                                   GTK_SIGNAL_FUNC(activate_callback),
                                   (gpointer) strdup(tv.v_string));
                break;
            case G_TOKEN_LEFT_CURLY:
                g_scanner_get_next_token(scanner);

                submenu = gtk_menu_new();
                append_menu_from_scanner(GTK_MENU(submenu),
                                         scanner, level + 1);
                gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);

                break;
            default:
                break;
            }
            break;
        case G_TOKEN_IDENTIFIER:
            tv = g_scanner_cur_value(scanner);
            if (strcmp(tv.v_identifier, "pty_list") == 0) {
                append_pty_list(GTK_MENU(menu));
            } else if (strcmp(tv.v_identifier, "-") == 0) {
                gtk_menu_append(GTK_MENU(menu), gtk_menu_item_new());
            } else {
                fprintf(stderr,
                        "%s: undefined identifier `%s'.\n",
                        progname, tv.v_identifier);
            }
            break;
        case G_TOKEN_RIGHT_CURLY:
            if ((level - 1) < 0) {
                fprintf(stderr, "%s: mismatched parentheses.\n", progname);
                return 0;
            } else {
                return 1;
            }
            break;
        default:
            break;
        }
    }

    return 1;
}

int append_pty_list(GtkMenu* menu)
{
    GtkWidget* item;
    char* pty_list;
    char* pty;
    char* command;
    int is_active;
    GSList* group = NULL;

    if ((pty_list = get_value("pty_list")) == NULL)
        return 1;

    while (pty_list) {
        pty = pty_list + 5;
        pty_list = strchr(pty_list, ':');
        if (pty_list)
            *pty_list = 0;
        else
            break;
        if (*(pty_list + 1) == '1')
            is_active = 1;
        else
            is_active = 0;
        pty_list = strchr(pty_list + 1, ';');
        if (pty_list)
            pty_list++;

        command = malloc(strlen(pty) + 17);
        sprintf(command, "select_pty=/dev/%s", pty);

        item = gtk_radio_menu_item_new_with_label(group, pty);
        group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(item));
        gtk_signal_connect(GTK_OBJECT(item), "activate",
                           GTK_SIGNAL_FUNC(activate_callback),
                           (gpointer) command);
        gtk_menu_append(menu, item);
        if (is_active) {
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),
                                           TRUE);
        }
    }

    return 1;
}


void activate_callback(GtkWidget* widget, gpointer data)
{
    char* command = data;

    printf("\x1b]5379;%s\x07", command);
}


char* get_value(char* key)
{
    int count;
    char ret[1024];
    char c;

    printf("\x1b]5381;%s\x07", key);
    fflush(stdout);

    for (count = 0; count < 1024; count++) {
        if (read(STDIN_FILENO, &c, 1) == 1) {
            if (c != '\n') {
                ret[count] = c;
            } else {
                ret[count] = '\0';

                if (count < 2 + strlen(key) || strcmp(ret, "#error") == 0) {
                    return NULL;
                }

                /*
                 * #key=value
                 */
                return strdup(ret + 2 + strlen(key));
            }
        } else {
#ifdef  DEBUG
            kik_debug_printf(KIK_DEBUG_TAG
                             " %s return from mlterm is illegal.\n", key);
#endif

            return NULL;
        }
    }

    return NULL;
}
