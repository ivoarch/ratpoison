/* functions for handling the window list 
 * Copyright (C) 2000, 2001 Shawn Betts
 *
 * This file is part of ratpoison.
 *
 * ratpoison is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * ratpoison is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "ratpoison.h"

LIST_HEAD(rp_unmapped_window);
LIST_HEAD(rp_mapped_window);

struct numset *rp_window_numset;

/* Get the mouse position relative to the root of the specified window */
static void
get_mouse_root_position (rp_window *win, int *mouse_x, int *mouse_y)
{
  Window root_win, child_win;
  int win_x, win_y;
  unsigned int mask;
  
  XQueryPointer (dpy, win->scr->root, &root_win, &child_win, &win_x, &win_y, mouse_x, mouse_y, &mask);
}

void
free_window (rp_window *w)
{
  if (w == NULL) return;

  free (w->user_name);
  free (w->res_name);
  free (w->res_class);
  free (w->wm_name);

  XFree (w->hints);

  free (w);
}

void
update_window_gravity (rp_window *win)
{
/*   if (win->hints->win_gravity == ForgetGravity) */
/*     { */
      if (win->transient)
	win->gravity = defaults.trans_gravity;
      else if (win->hints->flags & PMaxSize)
	win->gravity = defaults.maxsize_gravity;
      else
	win->gravity = defaults.win_gravity;
/*     } */
/*   else */
/*     { */
/*       win->gravity = win->hints->win_gravity; */
/*     } */
}

char *
window_name (rp_window *win)
{
  if (win == NULL) return NULL;

  if (win->named)
    return win->user_name;

  switch (defaults.win_name)
    {
    case WIN_NAME_RES_NAME:
      if (win->res_name)
	return win->res_name;
      else return win->user_name;
      
    case WIN_NAME_RES_CLASS:
      if (win->res_class)
	return win->res_class;
      else return win->user_name;

      /* if we're not looking for the res name or res class, then
	 we're looking for the window title. */
    default:
      if (win->wm_name)
	return win->wm_name;
      else return win->user_name;
    }

  return NULL;
}

/* Allocate a new window and add it to the list of managed windows */
rp_window *
add_to_window_list (screen_info *s, Window w)
{
  rp_window *new_window;

  new_window = xmalloc (sizeof (rp_window));

  new_window->w = w;
  new_window->scr = s;
  new_window->last_access = 0;
  new_window->state = WithdrawnState;
  new_window->number = -1;
  new_window->frame_number = EMPTY;
  new_window->named = 0;
  new_window->hints = XAllocSizeHints ();
  new_window->colormap = DefaultColormap (dpy, s->screen_num);
  new_window->transient = XGetTransientForHint (dpy, new_window->w, &new_window->transient_for);
  PRINT_DEBUG (("transient %d\n", new_window->transient));

  update_window_gravity (new_window);

  get_mouse_root_position (new_window, &new_window->mouse_x, &new_window->mouse_y);

  XSelectInput (dpy, new_window->w, WIN_EVENTS);

  new_window->user_name = xstrdup ("Unnamed");

  new_window->wm_name = NULL;
  new_window->res_name = NULL;
  new_window->res_class = NULL;

  /* Add the window to the end of the unmapped list. */
  list_add_tail (&new_window->node, &rp_unmapped_window);

  return new_window;
}

/* Check to see if the window is in the list of windows. */
rp_window *
find_window_in_list (Window w, struct list_head *list)
{
  rp_window *cur;

  list_for_each_entry (cur, list, node)
    {
      if (cur->w == w) return cur;
    }

  return NULL;
}

/* Check to see if the window is in any of the lists of windows. */
rp_window *
find_window (Window w)
{
  rp_window *win = NULL;

  win = find_window_in_list (w, &rp_mapped_window);

  if (!win) 
    {
      win = find_window_in_list (w, &rp_unmapped_window);
    }

  return win;
}

void
set_current_window (rp_window *win)
{
  set_frames_window (current_frame(), win);
}

rp_window *
find_window_number (int n)
{
  rp_window *cur;

  list_for_each_entry (cur,&rp_mapped_window,node)
    {
/*       if (cur->state == STATE_UNMAPPED) continue; */

      if (n == cur->number) return cur;
    }

  return NULL;
}

/* A case insensitive strncmp. */
static int
str_comp (char *s1, char *s2, int len)
{
  int i;

  for (i=0; i<len; i++)
    if (toupper (s1[i]) != toupper (s2[i])) return 0;

  return 1;
}

/* return a window by name */
rp_window *
find_window_name (char *name)
{
  rp_window *w;

  list_for_each_entry (w,&rp_mapped_window,node)
    {
/*       if (w->state == STATE_UNMAPPED)  */
/* 	continue; */

      if (str_comp (name, window_name (w), strlen (name))) 
	return w;
    }

  /* didn't find it */
  return NULL;
}

/* Return the previous window in the list. Assumes window is in the
   mapped window list. */
rp_window*
find_window_prev (rp_window *w)
{
  rp_window *cur;

  if (!w) return NULL;

  for (cur = list_prev_entry (w, &rp_mapped_window, node); 
       cur != w; 
       cur = list_prev_entry (cur, &rp_mapped_window, node))
    {
      if (!find_windows_frame (cur))
	{
	  return cur;
	}
    }

  return NULL;
}

/* Return the next window in the list. Assumes window is in the mapped
   window list. */
rp_window*
find_window_next (rp_window *w)
{
  rp_window *cur;

  if (!w) return NULL;

  for (cur = list_next_entry (w, &rp_mapped_window, node); 
       cur != w; 
       cur = list_next_entry (cur, &rp_mapped_window, node))
    {
      if (!find_windows_frame (cur))
	{
	  return cur;
	}
    }

  return NULL;
}

rp_window *
find_window_other ()
{
  int last_access = 0;
  rp_window *most_recent = NULL;
  rp_window *cur;

  list_for_each_entry (cur, &rp_mapped_window, node)
    {
      if (cur->last_access >= last_access 
	  && cur != current_window()
	  && !find_windows_frame (cur))
	{
	  most_recent = cur;
	  last_access = cur->last_access;
	}
    }

  return most_recent;
}

/* Assumes the list is sorted by increasing number. Inserts win into
   to Right place to keep the list sorted. */
void
insert_into_list (rp_window *win, struct list_head *list)
{
  rp_window *cur;

  list_for_each_entry (cur, list, node)
    {
      if (cur->number > win->number)
	{
	  list_add_tail (&win->node, &cur->node);
	  return;
	}
    }

  list_add_tail(&win->node, list);
}

static void
save_mouse_position (rp_window *win)
{
  Window root_win, child_win;
  int win_x, win_y;
  unsigned int mask;
  
  /* In the case the XQueryPointer raises a BadWindow error, the
     window is not mapped or has been destroyed so it doesn't matter
     what we store in mouse_x and mouse_y since they will never be
     used again. */

  ignore_badwindow++;

  XQueryPointer (dpy, win->w, &root_win, &child_win, 
		 &win->mouse_x, &win->mouse_y, &win_x, &win_y, &mask);

  ignore_badwindow--;
}

/* Takes focus away from last_win and gives focus to win */
void
give_window_focus (rp_window *win, rp_window *last_win)
{
  /* counter increments every time this function is called. This way
     we can track which window was last accessed. */
  static int counter = 1;

  if (win == NULL) return;

  counter++;
  win->last_access = counter;

  unhide_window (win);

  /* Warp the cursor to the window's saved position. */
  if (last_win != NULL) save_mouse_position (last_win);

  if (defaults.warp)
    XWarpPointer (dpy, None, win->scr->root, 
		  0, 0, 0, 0, win->mouse_x, win->mouse_y);
  
  /* Swap colormaps */
  if (last_win != NULL) XUninstallColormap (dpy, last_win->colormap);
  XInstallColormap (dpy, win->colormap);

  /* Finally, give the window focus */
  rp_current_screen = win->scr->screen_num;
  XSetInputFocus (dpy, win->w, 
		  RevertToPointerRoot, CurrentTime);

  XSync (dpy, False);
}

#if 0
void
unhide_transient_for (rp_window *win)
{
  rp_window_frame *frame;
  rp_window *transient_for;

  if (win == NULL) return;
  if (!win->transient) return;
  frame = find_windows_frame (win);

  transient_for = find_window (win->transient_for);
  if (transient_for == NULL) 
    {
      PRINT_DEBUG (("Can't find transient_for for '%s'", win->name ));
      return;
    }

  if (find_windows_frame (transient_for) == NULL)
    {
      set_frames_window (frame, transient_for);
      maximize (transient_for);

      PRINT_DEBUG (("unhide transient window: %s\n", transient_for->name));
      
      unhide_window_below (transient_for);

      if (transient_for->transient) 
	{
	  unhide_transient_for (transient_for);
	}

      set_frames_window (frame, win);
    }
  else if (transient_for->transient) 
    {
      unhide_transient_for (transient_for);
    }
}
#endif

#if 0
/* Hide all transient windows for win until we get to last. */
void
hide_transient_for_between (rp_window *win, rp_window *last)
{
  rp_window *transient_for;

  if (win == NULL) return;
  if (!win->transient) return;

  transient_for = find_window (win->transient_for);
  if (transient_for == last) 
    {
      PRINT_DEBUG (("Can't find transient_for for '%s'", win->name ));
      return;
    }

  if (find_windows_frame (transient_for) == NULL)
    {
      PRINT_DEBUG (("hide transient window: %s\n", transient_for->name));
      hide_window (transient_for);
    }

  if (transient_for->transient) 
    {
      hide_transient_for (transient_for);
    }
}
#endif

#if 0
void
hide_transient_for (rp_window *win)
{
  /* Hide ALL the transient windows for win. */
  hide_transient_for_between (win, NULL);
}
#endif

#if 0
/* return 1 if transient_for is a transient window for win or one of
   win's transient_for ancestors. */
int
is_transient_ancestor (rp_window *win, rp_window *transient_for)
{
  rp_window *tmp;

  if (win == NULL) return 0;
  if (!win->transient) return 0;
  if (transient_for == NULL) return 0;

  tmp = win;

  do
    {
      tmp = find_window (tmp->transient_for);
      if (tmp == transient_for)
	return 1;
    } while (tmp && tmp->transient);
  
  return 0;
}
#endif

/* In the current frame, set the active window to win. win will have focus. */
void
set_active_window (rp_window *win)
{
  rp_window *last_win;
  rp_window_frame *frame;

  if (win == NULL) return;

  frame = screen_get_frame (win->scr, win->scr->current_frame);
  last_win = set_frames_window (frame, win);

  if (last_win) PRINT_DEBUG (("last window: %s\n", window_name (last_win)));
  PRINT_DEBUG (("new window: %s\n", window_name (win)));

  /* Make sure the window comes up full screen */
  maximize (win);
  unhide_window (win);

#ifdef MAXSIZE_WINDOWS_ARE_TRANSIENTS
  if (!win->transient
      && !(win->hints->flags & PMaxSize 
	   && (win->hints->max_width < win->scr->root_attr.width
	       || win->hints->max_height < win->scr->root_attr.height)))
#else
  if (!win->transient)
#endif
    hide_others(win);

  give_window_focus (win, last_win);

  /* Make sure the program bar is always on the top */
  update_window_names (win->scr);

  XSync (dpy, False);
}

/* Go to the window, switching frames if the window is already in a
   frame. */
void
goto_window (rp_window *win)
{
  rp_window_frame *frame;

  frame = find_windows_frame (win);
  if (frame)
    {
      set_active_frame (frame);
    }
  else
    {
      set_active_window (win);
    }
}

void
print_window_information (rp_window *win)
{
  marked_message_printf (0, 0, MESSAGE_WINDOW_INFORMATION, 
			 win->number, window_name (win));
}

/* format options
   %n - Window number
   %s - Window status (current window, last window, etc)
   %t - Window Name
   %a - application name
   %c - resource class
   %i - X11 Window ID
   %l - last access number
   
 */
static void
format_window_name (char *fmt, rp_window *win, rp_window *other_win, 
		    struct sbuf *buffer)
{
  int esc = 0;
  char dbuf[10];

  for(; *fmt; fmt++)
    {
      if (*fmt == '%' && !esc)
	{
	  esc = 1;
	  continue;
	}

      if (esc)
	{
	  switch (*fmt)
	    {
	    case 'n':
	      snprintf (dbuf, 10, "%d", win->number);
	      sbuf_concat (buffer, dbuf);
	      break;

	    case 's':
	      if (win == current_window())
		sbuf_concat (buffer, "*");
	      else if (win == other_win)
		sbuf_concat (buffer, "+");
	      else
		sbuf_concat (buffer, "-");
	      break;

	    case 't':
	      sbuf_concat (buffer, window_name (win));
	      break;

	    case 'a':
	      if (win->res_name)
		sbuf_concat (buffer, win->res_name);
	      else
		sbuf_concat (buffer, "None");
	      break;

	    case 'c':
	      if (win->res_class)
		sbuf_concat (buffer, win->res_class);
	      else
		sbuf_concat (buffer, "None");
	      break;

	    case 'i':
	      snprintf (dbuf, 9, "%ld", (unsigned long)win->w);
	      sbuf_concat (buffer, dbuf);
	      break;

	    case 'l':
	      snprintf (dbuf, 9, "%d", win->last_access);
	      sbuf_concat (buffer, dbuf);
	      break;

	    case '%':
	      sbuf_concat (buffer, "%");
	      break;

	    default:
	      sbuf_printf_concat (buffer, "%%%c", *fmt);
	      break;
	    }

	  /* Reset the 'escape' state. */
	  esc = 0;
	}
      else
	{
	  /* Insert the character. */
	  dbuf[0] = *fmt;
	  dbuf[1] = 0;
	  sbuf_concat (buffer, dbuf);
	}
    }
}

/* get the window list and store it in buffer delimiting each window
   with delim. mark_start and mark_end will be filled with the text
   positions for the start and end of the current window. */
void
get_window_list (char *fmt, char *delim, struct sbuf *buffer, 
		 int *mark_start, int *mark_end)
{
  rp_window *w;
  rp_window *other_window;

  if (buffer == NULL) return;

  sbuf_clear (buffer);
  other_window = find_window_other ();

  list_for_each_entry (w,&rp_mapped_window,node)
    {
      PRINT_DEBUG (("%d-%s\n", w->number, window_name (w)));

      if (w == current_window())
	*mark_start = strlen (sbuf_get (buffer));

      /* A hack, pad the window with a space at the beginning and end
         if there is no delimiter. */
      if (!delim)
	sbuf_concat (buffer, " ");

      format_window_name (fmt, w, other_window, buffer);

      /* A hack, pad the window with a space at the beginning and end
         if there is no delimiter. */
      if (!delim)
	sbuf_concat (buffer, " ");

      /* Only put the delimiter between the windows, and not after the the last
         window. */
      if (delim && w->node.next != &rp_mapped_window)
	sbuf_concat (buffer, delim);

      if (w == current_window()) {
	if(defaults.window_list_style == STYLE_ROW){
	  *mark_end = strlen (sbuf_get (buffer));
	} else {
	  *mark_end = strlen (sbuf_get(buffer)) - *mark_start;
	}
      }
    }

  if (!strcmp (sbuf_get (buffer), ""))
    {
      sbuf_copy (buffer, MESSAGE_NO_MANAGED_WINDOWS);
    }
}

void
init_window_stuff ()
{
  rp_window_numset = numset_new ();
}

void
free_window_stuff ()
{
  numset_free (rp_window_numset);
}

rp_window_frame *
win_get_frame (rp_window *win)
{
  if (win->frame_number != EMPTY)
    return screen_get_frame (win->scr, win->frame_number);
  else
    return NULL;
}
