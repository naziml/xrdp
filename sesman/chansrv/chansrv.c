/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   xrdp: A Remote Desktop Protocol server.
   Copyright (C) Jay Sorg 2009
*/

#include <X11/Xlib.h>
#include "arch.h"
#include "os_calls.h"
#include "thread_calls.h"
#include "trans.h"
#include "chansrv.h"
#include "defines.h"
#include "sound.h"
#include "clipboard.h"
#include "devredir.h"

static tbus g_thread_done_event = 0;
static struct trans* g_lis_trans = 0;
static struct trans* g_con_trans = 0;
static struct chan_item g_chan_items[32];
static int g_num_chan_items = 0;
static int g_cliprdr_index = -1;
static int g_rdpsnd_index = -1;
static int g_rdpdr_index = -1;

tbus g_term_event = 0;
int g_display_num = 0;
int g_cliprdr_chan_id = -1; /* cliprdr */
int g_rdpsnd_chan_id = -1; /* rdpsnd */
int g_rdpdr_chan_id = -1; /* rdpdr */

Display* g_display = 0;

/*****************************************************************************/
/* returns error */
int APP_CC
send_channel_data(int chan_id, char* data, int size)
{
  struct stream* s;
  int chan_flags;
  int total_size;
  int sent;
  int rv;

  s = trans_get_out_s(g_con_trans, 8192);
  if (s == 0)
  {
    return 1;
  }
  rv = 0;
  sent = 0;
  total_size = size;
  while (sent < total_size)
  {
    size = MIN(1600, total_size - sent);
    chan_flags = 0;
    if (sent == 0)
    {
      chan_flags |= 1; /* first */
    }
    if (size + sent == total_size)
    {
      chan_flags |= 2; /* last */
    }
    out_uint32_le(s, 0); /* version */
    out_uint32_le(s, 8 + 8 + 2 + 2 + 2 + 4 + size); /* size */
    out_uint32_le(s, 8); /* msg id */
    out_uint32_le(s, 8 + 2 + 2 + 2 + 4 + size); /* size */
    out_uint16_le(s, chan_id);
    out_uint16_le(s, chan_flags);
    out_uint16_le(s, size);
    out_uint32_le(s, total_size);
    out_uint8a(s, data + sent, size);
    s_mark_end(s);
    rv = trans_force_write(g_con_trans);
    if (rv != 0)
    {
      break;
    }
    sent += size;
    s = trans_get_out_s(g_con_trans, 8192);
  }
  return rv;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
send_init_response_message(void)
{
  struct stream* s;

  LOG(1, ("send_init_response_message:"));
  s = trans_get_out_s(g_con_trans, 8192);
  if (s == 0)
  {
    return 1;
  }
  out_uint32_le(s, 0); /* version */
  out_uint32_le(s, 8 + 8); /* size */
  out_uint32_le(s, 2); /* msg id */
  out_uint32_le(s, 8); /* size */
  s_mark_end(s);
  return trans_force_write(g_con_trans);
}

/*****************************************************************************/
/* returns error */
static int APP_CC
send_channel_setup_response_message(void)
{
  struct stream* s;

  LOG(10, ("send_channel_setup_response_message:"));
  s = trans_get_out_s(g_con_trans, 8192);
  if (s == 0)
  {
    return 1;
  }
  out_uint32_le(s, 0); /* version */
  out_uint32_le(s, 8 + 8); /* size */
  out_uint32_le(s, 4); /* msg id */
  out_uint32_le(s, 8); /* size */
  s_mark_end(s);
  return trans_force_write(g_con_trans);
}

/*****************************************************************************/
/* returns error */
static int APP_CC
send_channel_data_response_message(void)
{
  struct stream* s;

  LOG(10, ("send_channel_data_response_message:"));
  s = trans_get_out_s(g_con_trans, 8192);
  if (s == 0)
  {
    return 1;
  }
  out_uint32_le(s, 0); /* version */
  out_uint32_le(s, 8 + 8); /* size */
  out_uint32_le(s, 6); /* msg id */
  out_uint32_le(s, 8); /* size */
  s_mark_end(s);
  return trans_force_write(g_con_trans);
}

/*****************************************************************************/
/* returns error */
static int APP_CC
process_message_init(struct stream* s)
{
  LOG(10, ("process_message_init:"));
  return send_init_response_message();
}

/*****************************************************************************/
/* returns error */
static int APP_CC
process_message_channel_setup(struct stream* s)
{
  int num_chans;
  int index;
  int rv;
  struct chan_item* ci;

  LOG(10, ("process_message_channel_setup:"));
  in_uint16_le(s, num_chans);
  LOG(10, ("process_message_channel_setup: num_chans %d", num_chans));
  for (index = 0; index < num_chans; index++)
  {
    ci = &(g_chan_items[g_num_chan_items]);
    g_memset(ci->name, 0, sizeof(ci->name));
    in_uint8a(s, ci->name, 8);
    in_uint16_le(s, ci->id);
    in_uint16_le(s, ci->flags);
    LOG(10, ("process_message_channel_setup: chan name '%s' "
             "id %d flags %8.8x", ci->name, ci->id, ci->flags));
    if (g_strcasecmp(ci->name, "cliprdr") == 0)
    {
      g_cliprdr_index = g_num_chan_items;
      g_cliprdr_chan_id = ci->id;
    }
    else if (g_strcasecmp(ci->name, "rdpsnd") == 0)
    {
      g_rdpsnd_index = g_num_chan_items;
      g_rdpsnd_chan_id = ci->id;
    }
    else if (g_strcasecmp(ci->name, "rdpdr") == 0)
    {
      g_rdpdr_index = g_num_chan_items;
      g_rdpdr_chan_id = ci->id;
    }
    g_num_chan_items++;
  }
  rv = send_channel_setup_response_message();
  if (g_cliprdr_index >= 0)
  {
    clipboard_init();
  }
  if (g_rdpsnd_index >= 0)
  {
    sound_init();
  }
  if (g_rdpdr_index >= 0)
  {
    dev_redir_init();
  }
  return rv;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
process_message_channel_data(struct stream* s)
{
  int chan_id;
  int chan_flags;
  int rv;
  int length;
  int total_length;

  in_uint16_le(s, chan_id);
  in_uint16_le(s, chan_flags);
  in_uint16_le(s, length);
  in_uint32_le(s, total_length);
  LOG(10, ("process_message_channel_data: chan_id %d "
           "chan_flags %d", chan_id, chan_flags));
  rv = send_channel_data_response_message();
  if (rv == 0)
  {
    if (chan_id == g_cliprdr_chan_id)
    {
      rv = clipboard_data_in(s, chan_id, chan_flags, length, total_length);
    }
    else if (chan_id == g_rdpsnd_chan_id)
    {
      rv = sound_data_in(s, chan_id, chan_flags, length, total_length);
    }
    else if (chan_id == g_rdpdr_chan_id)
    {
      rv = dev_redir_data_in(s, chan_id, chan_flags, length, total_length);
    }
  }
  return rv;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
process_message_channel_data_response(struct stream* s)
{
  LOG(10, ("process_message_channel_data_response:"));
  return 0;
}

/*****************************************************************************/
/* returns error */
static int APP_CC
process_message(void)
{
  struct stream* s;
  int size;
  int id;
  int rv;
  char* next_msg;

  if (g_con_trans == 0)
  {
    return 1;
  }
  s = trans_get_in_s(g_con_trans);
  if (s == 0)
  {
    return 1;
  }
  rv = 0;
  while (s_check_rem(s, 8))
  {
    next_msg = s->p;
    in_uint32_le(s, id);
    in_uint32_le(s, size);
    next_msg += size;
    switch (id)
    {
      case 1: /* init */
        rv = process_message_init(s);
        break;
      case 3: /* channel setup */
        rv = process_message_channel_setup(s);
        break;
      case 5: /* channel data */
        rv = process_message_channel_data(s);
        break;
      case 7: /* channel data response */
        rv = process_message_channel_data_response(s);
        break;
      default:
        LOG(0, ("process_message: error in process_message "
                "unknown msg %d", id));
        break;
    }
    if (rv != 0)
    {
      break;
    }
    s->p = next_msg;
  }
  return rv;
}

/*****************************************************************************/
/* returns error */
int DEFAULT_CC
my_trans_data_in(struct trans* trans)
{
  struct stream* s;
  int id;
  int size;
  int error;

  if (trans == 0)
  {
    return 0;
  }
  if (trans != g_con_trans)
  {
    return 1;
  }
  LOG(10, ("my_trans_data_in:"));
  s = trans_get_in_s(trans);
  in_uint32_le(s, id);
  in_uint32_le(s, size);
  error = trans_force_read(trans, size - 8);
  if (error == 0)
  {
    /* here, the entire message block is read in, process it */
    error = process_message();
  }
  return error;
}

/*****************************************************************************/
int DEFAULT_CC
my_trans_conn_in(struct trans* trans, struct trans* new_trans)
{
  if (trans == 0)
  {
    return 1;
  }
  if (trans != g_lis_trans)
  {
    return 1;
  }
  if (g_con_trans != 0) /* if already set, error */
  {
    return 1;
  }
  if (new_trans == 0)
  {
    return 1;
  }
  LOG(10, ("my_trans_conn_in:"));
  g_con_trans = new_trans;
  g_con_trans->trans_data_in = my_trans_data_in;
  g_con_trans->header_size = 8;
  /* stop listening */
  trans_delete(g_lis_trans);
  g_lis_trans = 0;
  return 0;
}

/*****************************************************************************/
static int APP_CC
setup_listen(void)
{
  char text[256];
  int error;

  if (g_lis_trans != 0)
  {
    trans_delete(g_lis_trans);
  }
  g_lis_trans = trans_create(1, 8192, 8192);
  g_lis_trans->trans_conn_in = my_trans_conn_in;
  g_snprintf(text, 255, "%d", 7200 + g_display_num);
  error = trans_listen(g_lis_trans, text);
  if (error != 0)
  {
    LOG(0, ("setup_listen: trans_listen failed"));
    return 1;
  }
  return 0;
}

/*****************************************************************************/
THREAD_RV THREAD_CC
channel_thread_loop(void* in_val)
{
  tbus objs[32];
  int num_objs;
  int timeout;
  int error;
  THREAD_RV rv;

  LOG(1, ("channel_thread_loop: thread start"));
  rv = 0;
  error = setup_listen();
  if (error == 0)
  {
    timeout = 0;
    num_objs = 0;
    objs[num_objs] = g_term_event;
    num_objs++;
    trans_get_wait_objs(g_lis_trans, objs, &num_objs, &timeout);
    while (g_obj_wait(objs, num_objs, 0, 0, timeout) == 0)
    {
      if (g_is_wait_obj_set(g_term_event))
      {
        LOG(0, ("channel_thread_loop: g_term_event set"));
        clipboard_deinit();
        sound_deinit();
        dev_redir_deinit();
        break;
      }
      if (g_lis_trans != 0)
      {
        if (trans_check_wait_objs(g_lis_trans) != 0)
        {
          LOG(0, ("channel_thread_loop: trans_check_wait_objs error"));
        }
      }
      if (g_con_trans != 0)
      {
        if (trans_check_wait_objs(g_con_trans) != 0)
        {
          LOG(0, ("channel_thread_loop: "
                  "trans_check_wait_objs error resetting"));
          /* delete g_con_trans */
          trans_delete(g_con_trans);
          g_con_trans = 0;
          /* create new listener */
          error = setup_listen();
          if (error != 0)
          {
            break;
          }
        }
      }
      clipboard_check_wait_objs();
      sound_check_wait_objs();
      dev_redir_check_wait_objs();
      timeout = 0;
      num_objs = 0;
      objs[num_objs] = g_term_event;
      num_objs++;
      trans_get_wait_objs(g_lis_trans, objs, &num_objs, &timeout);
      trans_get_wait_objs(g_con_trans, objs, &num_objs, &timeout);
      clipboard_get_wait_objs(objs, &num_objs, &timeout);
      sound_get_wait_objs(objs, &num_objs, &timeout);
      dev_redir_get_wait_objs(objs, &num_objs, &timeout);
    }
  }
  trans_delete(g_lis_trans);
  g_lis_trans = 0;
  trans_delete(g_con_trans);
  g_con_trans = 0;
  LOG(0, ("channel_thread_loop: thread stop"));
  g_set_wait_obj(g_thread_done_event);
  return rv;
}

/*****************************************************************************/
void DEFAULT_CC
term_signal_handler(int sig)
{
  LOG(1, ("term_signal_handler: got signal %d", sig));
  g_set_wait_obj(g_term_event);
}

/*****************************************************************************/
void DEFAULT_CC
nil_signal_handler(int sig)
{
  LOG(1, ("nil_signal_handler: got signal %d", sig));
}

/*****************************************************************************/
static int APP_CC
get_display_num_from_display(char* display_text)
{
  int index;
  int mode;
  int host_index;
  int disp_index;
  int scre_index;
  char host[256];
  char disp[256];
  char scre[256];

  index = 0;
  host_index = 0;
  disp_index = 0;
  scre_index = 0;
  mode = 0;
  while (display_text[index] != 0)
  {
    if (display_text[index] == ':')
    {
      mode = 1;
    }
    else if (display_text[index] == '.')
    {
      mode = 2;
    }
    else if (mode == 0)
    {
      host[host_index] = display_text[index];
      host_index++;
    }
    else if (mode == 1)
    {
      disp[disp_index] = display_text[index];
      disp_index++;
    }
    else if (mode == 2)
    {
      scre[scre_index] = display_text[index];
      scre_index++;
    }
    index++;
  }
  host[host_index] = 0;
  disp[disp_index] = 0;
  scre[scre_index] = 0;
  g_display_num = g_atoi(disp);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
my_error_handler(Display* dis, XErrorEvent* xer)
{
  char text[256];

  XGetErrorText(dis, xer->error_code, text, 255);
  LOG(1, ("error [%s]", text));
  return 0;
}

/*****************************************************************************/
/* The X server had an internal error.  This is the last function called.
   Do any cleanup that needs to be done on exit, like removing temporary files.
   Don't worry about memory leaks */
int DEFAULT_CC
my_fatal_handler(Display* dis)
{
  LOG(1, ("fatal error, exiting"));
  g_delete_wait_obj(g_term_event);
  g_delete_wait_obj(g_thread_done_event);
  return 0;
}

/*****************************************************************************/
int DEFAULT_CC
main(int argc, char** argv)
{
  int pid;
  char text[256];
  char* display_text;

  g_init(); /* os_calls */
  pid = g_getpid();
  LOG(1, ("main: app started pid %d(0x%8.8x)", pid, pid));
  g_signal_kill(term_signal_handler); /* SIGKILL */
  g_signal_terminate(term_signal_handler); /* SIGTERM */
  g_signal_user_interrupt(term_signal_handler); /* SIGINT */
  g_signal_pipe(nil_signal_handler); /* SIGPIPE */
  display_text = g_getenv("DISPLAY");
  LOG(1, ("main: DISPLAY env var set to %s", display_text));
  get_display_num_from_display(display_text);
  if (g_display_num == 0)
  {
    LOG(0, ("main: error, display is zero"));
    return 1;
  }
  LOG(1, ("main: using DISPLAY %d", g_display_num));
  g_display = XOpenDisplay(0);
  if (g_display == 0)
  {
    LOG(0, ("main: XOpenDisplay failed"));
    return 1;
  }
  XSetErrorHandler(my_error_handler);
  XSetIOErrorHandler(my_fatal_handler);
  g_snprintf(text, 255, "xrdp_chansrv_%8.8x_main_term", pid);
  g_term_event = g_create_wait_obj(text);
  g_snprintf(text, 255, "xrdp_chansrv_%8.8x_thread_done", pid);
  g_thread_done_event = g_create_wait_obj(text);
  tc_thread_create(channel_thread_loop, 0);
  while (!g_is_wait_obj_set(g_term_event))
  {
    if (g_obj_wait(&g_term_event, 1, 0, 0, 0) != 0)
    {
      LOG(0, ("main: error, g_obj_wait failed"));
      break;
    }
  }
  while (!g_is_wait_obj_set(g_thread_done_event))
  {
    /* wait for thread to exit */
    if (g_obj_wait(&g_thread_done_event, 1, 0, 0, 0) != 0)
    {
      LOG(0, ("main: error, g_obj_wait failed"));
      break;
    }
  }
  /* cleanup */
  g_delete_wait_obj(g_term_event);
  g_delete_wait_obj(g_thread_done_event);
  LOG(1, ("main: app exiting pid %d(0x%8.8x)", pid, pid));
  g_deinit(); /* os_calls */
  return 0;
}