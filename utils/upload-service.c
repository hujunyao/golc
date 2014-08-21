/*
     This file is part of libmicrohttpd
     (C) 2013 Christian Grothoff (and other contributing authors)

     This library is free software; you can redistribute it and/or
     modify it under the terms of the GNU Lesser General Public
     License as published by the Free Software Foundation; either
     version 2.1 of the License, or (at your option) any later version.

     This library is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     Lesser General Public License for more details.

     You should have received a copy of the GNU Lesser General Public
     License along with this library; if not, write to the Free Software
     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/**
 * @file demo.c
 * @brief complex demonstration site: create directory index, offer
 *        upload via form and HTTP POST, download with mime type detection
 *        and error reporting (403, etc.) --- and all of this with
 *        high-performance settings (large buffers, thread pool).
 *        If you want to benchmark MHD, this code should be used to
 *        run tests against.  Note that the number of threads may need
 *        to be adjusted depending on the number of available cores.
 * @author Christian Grothoff
 */
//#include "platform.h"
#include <microhttpd.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <magic.h>
#include <limits.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>

#if defined(CPU_COUNT) && (CPU_COUNT+0) < 2
#undef CPU_COUNT
#endif
#if !defined(CPU_COUNT)
#define CPU_COUNT 2
#endif

/**
 * Number of threads to run in the thread pool.  Should (roughly) match
 * the number of cores on your system.
 */
#define NUMBER_OF_THREADS CPU_COUNT

/**
 * How many bytes of a file do we give to libmagic to determine the mime type?
 * 16k might be a bit excessive, but ought not hurt performance much anyway,
 * and should definitively be on the safe side.
 */
#define MAGIC_HEADER_SIZE (16 * 1024)


/**
 * Page returned for file-not-found.
 */
#define FILE_NOT_FOUND_PAGE "<html><head><title>File not found</title></head><body>File not found</body></html>"


/**
 * Page returned for internal errors.
 */
#define INTERNAL_ERROR_PAGE "<html><head><title>Internal error</title></head><body>Internal error</body></html>"


/**
 * Page returned for refused requests.
 */
#define REQUEST_REFUSED_PAGE "<html><head><title>Request refused</title></head><body>Request refused (file exists?)</body></html>"


/**
 * Head of index page.
 */
#define INDEX_PAGE_HEADER "<html>\n<head><title>Welcome</title></head>\n<body>\n"\
   "<h1>Upload</h1>\n"\
   "<form method=\"POST\" enctype=\"multipart/form-data\" action=\"/\">\n"\
   "<dl><dt>Content type:</dt><dd>"\
   "<input type=\"radio\" name=\"from_jid\" value=\"books\">Book</input>"\
   "<input type=\"radio\" name=\"from_jid\" value=\"images\">Image</input>"\
   "<input type=\"radio\" name=\"from_jid\" value=\"music\">Music</input>"\
   "<input type=\"radio\" name=\"from_jid\" value=\"software\">Software</input>"\
   "<input type=\"radio\" name=\"from_jid\" value=\"videos\">Videos</input>\n"\
   "<input type=\"radio\" name=\"from_jid\" value=\"other\" checked>Other</input></dd>"\
   "<dt>Language:</dt><dd>"\
   "<input type=\"radio\" name=\"to_jid\" value=\"no-lang\" checked>none</input>"\
   "<input type=\"radio\" name=\"to_jid\" value=\"en\">English</input>"\
   "<input type=\"radio\" name=\"to_jid\" value=\"de\">German</input>"\
   "<input type=\"radio\" name=\"to_jid\" value=\"fr\">French</input>"\
   "<input type=\"radio\" name=\"to_jid\" value=\"es\">Spanish</input></dd>\n"\
   "<dt>File:</dt><dd>"\
   "<input type=\"file\" name=\"upload\"/></dd></dl>"\
   "<input type=\"submit\" value=\"Send!\"/>\n"\
   "</form>\n"\
   "<h1>Download</h1>\n"\
   "<ol>\n"

/**
 * Footer of index page.
 */
#define INDEX_PAGE_FOOTER "</ol>\n</body>\n</html>"


/**
 * Response returned if the requested file does not exist (or is not accessible).
 */
static struct MHD_Response *file_not_found_response;

/**
 * Response returned for internal errors.
 */
static struct MHD_Response *internal_error_response;

/**
 * Response returned for '/' (GET) to list the contents of the directory and allow upload.
 */
static struct MHD_Response *cached_directory_response;

/**
 * Response returned for refused uploads.
 */
static struct MHD_Response *request_refused_response;

/**
 * Mutex used when we update the cached directory response object.
 */
static pthread_mutex_t mutex;

/**
 * Global handle to MAGIC data.
 */
static magic_t magic;


/**
 * Mark the given response as HTML for the brower.
 *
 * @param response response to mark
 */
static void
mark_as_html (struct MHD_Response *response)
{
  (void) MHD_add_response_header (response,
				  MHD_HTTP_HEADER_CONTENT_TYPE,
				  "text/html");
}


/**
 * Replace the existing 'cached_directory_response' with the
 * given response.
 *
 * @param response new directory response
 */
static void
update_cached_response (struct MHD_Response *response)
{
  (void) pthread_mutex_lock (&mutex);
  if (NULL != cached_directory_response)
    MHD_destroy_response (cached_directory_response);
  cached_directory_response = response;
  (void) pthread_mutex_unlock (&mutex);
}


/**
 * Context keeping the data for the response we're building.
 */
struct ResponseDataContext
{
  /**
   * Response data string.
   */
  char *buf;

  /**
   * Number of bytes allocated for 'buf'.
   */
  size_t buf_len;

  /**
   * Current position where we append to 'buf'. Must be smaller or equal to 'buf_len'.
   */
  size_t off;

};


/**
 * Context we keep for an upload.
 */
struct UploadContext
{
  /**
   * Handle where we write the uploaded file to.
   */
  int fd;

  /**
   * Name of the file on disk (used to remove on errors).
   */
  char *filename;

  char *from_jid;
  char *to_jid;

  /**
   * Post processor we're using to process the upload.
   */
  struct MHD_PostProcessor *pp;

  /**
   * Handle to connection that we're processing the upload for.
   */
  struct MHD_Connection *connection;

  /**
   * Response to generate, NULL to use directory.
   */
  struct MHD_Response *response;
};


/**
 * Append the 'size' bytes from 'data' to '*ret', adding
 * 0-termination.  If '*ret' is NULL, allocate an empty string first.
 *
 * @param ret string to update, NULL or 0-terminated
 * @param data data to append
 * @param size number of bytes in 'data'
 * @return MHD_NO on allocation failure, MHD_YES on success
 */
static int
do_append (char **ret,
	   const char *data,
	   size_t size)
{
  char *buf;
  size_t old_len;

  if (NULL == *ret)
    old_len = 0;
  else
    old_len = strlen (*ret);
  buf = malloc (old_len + size + 1);
  if (NULL == buf)
    return MHD_NO;
  memcpy (buf, *ret, old_len);
  if (NULL != *ret)
    free (*ret);
  memcpy (&buf[old_len], data, size);
  buf[old_len + size] = '\0';
  *ret = buf;
  return MHD_YES;
}


/**
 * Iterator over key-value pairs where the value
 * maybe made available in increments and/or may
 * not be zero-terminated.  Used for processing
 * POST data.
 *
 * @param cls user-specified closure
 * @param kind type of the value, always MHD_POSTDATA_KIND when called from MHD
 * @param key 0-terminated key for the value
 * @param filename name of the uploaded file, NULL if not known
 * @param content_type mime-type of the data, NULL if not known
 * @param transfer_encoding encoding of the data, NULL if not known
 * @param data pointer to size bytes of data at the
 *              specified offset
 * @param off offset of data in the overall value
 * @param size number of bytes in data available
 * @return MHD_YES to continue iterating,
 *         MHD_NO to abort the iteration
 */
static int
process_upload_data (void *cls,
		     enum MHD_ValueKind kind,
		     const char *key,
		     const char *filename,
		     const char *content_type,
		     const char *transfer_encoding,
		     const char *data,
		     uint64_t off,
		     size_t size)
{
  struct UploadContext *uc = cls;
  int i;

  //fprintf(stderr, "key = %s, filename = %s\n", key, filename);
  if (0 == strcmp(key, "from_jid"))
    return do_append (&uc->from_jid, data, size);
  if (0 == strcmp(key, "to_jid"))
    return do_append (&uc->to_jid, data, size);
  if (0 != strcmp (key, "upload"))
    {
      fprintf (stderr,
	       "Ignoring unexpected form value `%s'\n",
	       key);
      return MHD_YES; /* ignore */
    }
  if (NULL == filename)
    {
      fprintf (stderr, "No filename, aborting upload\n");
      return MHD_NO; /* no filename, error */
    }
  if ( (NULL == uc->to_jid) ||
       (NULL == uc->from_jid) )
    {
      fprintf (stderr,
	       "Missing form data for upload `%s'\n",
	       filename);
      uc->response = request_refused_response;
      return MHD_NO;
    }
  if (-1 == uc->fd)
    {
      char fn[PATH_MAX];

      if ( (NULL != strstr (filename, "..")) ||
	   (NULL != strchr (filename, '/')) ||
	   (NULL != strchr (filename, '\\')) )
	{
	  uc->response = request_refused_response;
	  return MHD_NO;
	}
      /* create directories -- if they don't exist already */
      (void) mkdir (uc->to_jid, S_IRWXU);
      snprintf (fn, sizeof (fn),
		"%s/%s",
		uc->to_jid,
		uc->from_jid);

      (void) mkdir (fn, S_IRWXU);

      /* open file */
      snprintf (fn, sizeof (fn),
		"%s/%s/%s",
		uc->to_jid,
		uc->from_jid,
		filename);
      for (i=strlen (fn)-1;i>=0;i--)
	if (! isprint ((int) fn[i]))
	  fn[i] = '_';
      uc->fd = open (fn,
		     O_CREAT | O_EXCL
#if O_LARGEFILE
		     | O_LARGEFILE
#endif
		     | O_WRONLY,
		     S_IRUSR | S_IWUSR);
      if (-1 == uc->fd)
	{
	  fprintf (stderr,
		   "Error opening file `%s' for upload: %s\n",
		   fn,
		   strerror (errno));
	  uc->response = request_refused_response;
	  return MHD_NO;
	}
      uc->filename = strdup (fn);
    }
  if ( (0 != size) &&
       (size != (size_t) write (uc->fd, data, size)) )
    {
      /* write failed; likely: disk full */
      fprintf (stderr,
	       "Error writing to file `%s': %s\n",
	       uc->filename,
	       strerror (errno));
      uc->response = internal_error_response;
      close (uc->fd);
      uc->fd = -1;
      if (NULL != uc->filename)
	{
	  unlink (uc->filename);
	  free (uc->filename);
	  uc->filename = NULL;
	}
      return MHD_NO;
    }
  return MHD_YES;
}


/**
 * Function called whenever a request was completed.
 * Used to clean up 'struct UploadContext' objects.
 *
 * @param cls client-defined closure, NULL
 * @param connection connection handle
 * @param con_cls value as set by the last call to
 *        the MHD_AccessHandlerCallback, points to NULL if this was
 *            not an upload
 * @param toe reason for request termination
 */
static void
response_completed_callback (void *cls,
			     struct MHD_Connection *connection,
			     void **con_cls,
			     enum MHD_RequestTerminationCode toe)
{
  struct UploadContext *uc = *con_cls;

  if (NULL == uc)
    return; /* this request wasn't an upload request */
  if (NULL != uc->pp)
    {
      MHD_destroy_post_processor (uc->pp);
      uc->pp = NULL;
    }
  if (-1 != uc->fd)
  {
    (void) close (uc->fd);
    if (NULL != uc->filename)
      {
	fprintf (stderr,
		 "Upload of file `%s' failed (incomplete or aborted), removing file.\n",
		 uc->filename);
	(void) unlink (uc->filename);
      }
  }
  if (NULL != uc->filename)
    free (uc->filename);
  free (uc);
}


/**
 * Return the current directory listing.
 *
 * @param connection connection to return the directory for
 * @return MHD_YES on success, MHD_NO on error
 */
static int
return_directory_response (struct MHD_Connection *connection)
{
  int ret;

  (void) pthread_mutex_lock (&mutex);
  if (NULL == cached_directory_response)
    ret = MHD_queue_response (connection,
			      MHD_HTTP_INTERNAL_SERVER_ERROR,
			      internal_error_response);
  else
    ret = MHD_queue_response (connection,
			      MHD_HTTP_OK,
			      cached_directory_response);
  (void) pthread_mutex_unlock (&mutex);
  return ret;
}


/**
 * Main callback from MHD, used to generate the page.
 *
 * @param cls NULL
 * @param connection connection handle
 * @param url requested URL
 * @param method GET, PUT, POST, etc.
 * @param version HTTP version
 * @param upload_data data from upload (PUT/POST)
 * @param upload_data_size number of bytes in "upload_data"
 * @param ptr our context
 * @return MHD_YES on success, MHD_NO to drop connection
 */
static int
generate_page (void *cls,
	       struct MHD_Connection *connection,
	       const char *url,
	       const char *method,
	       const char *version,
	       const char *upload_data,
	       size_t *upload_data_size, void **ptr)
{
  struct MHD_Response *response;
  int ret;
  int fd;
  struct stat buf;

  //fprintf(stderr, "url = %s, method = %s\n", url, method);

  if (0 == strcmp (method, MHD_HTTP_METHOD_POST)) {
      struct UploadContext *uc = *ptr;

      if (NULL == uc) {
	  if (NULL == (uc = malloc (sizeof (struct UploadContext))))
	    return MHD_NO; /* out of memory, close connection */
	  memset (uc, 0, sizeof (struct UploadContext));
          uc->fd = -1;
	  uc->connection = connection;
	  uc->pp = MHD_create_post_processor (connection,
					      64 * 1024 /* buffer size */,
					      &process_upload_data, uc);
	  if (NULL == uc->pp)
	    {
	      /* out of memory, close connection */
	      free (uc);
	      return MHD_NO;
	    }
	  *ptr = uc;
	  return MHD_YES;
	}
      if (0 != *upload_data_size) {
	  if (NULL == uc->response)
	    (void) MHD_post_process (uc->pp,
				     upload_data,
				     *upload_data_size);
	  *upload_data_size = 0;
	  return MHD_YES;
	}
      /* end of upload, finish it! */
      MHD_destroy_post_processor (uc->pp);
      uc->pp = NULL;
      if (-1 != uc->fd)
	{
	  close (uc->fd);
	  uc->fd = -1;
	}
      if (NULL != uc->response)
	{
	  return MHD_queue_response (connection,
				     MHD_HTTP_FORBIDDEN,
				     uc->response);
	} else {
	  return return_directory_response (connection);
	}
    }
  if (0 == strcmp (method, MHD_HTTP_METHOD_GET))
  {
    return return_directory_response (connection);
  }

  /* unexpected request, refuse */
  return MHD_queue_response (connection,
			     MHD_HTTP_FORBIDDEN,
			     request_refused_response);
}


/**
 * Function called if we get a SIGPIPE. Does nothing.
 *
 * @param sig will be SIGPIPE (ignored)
 */
static void
catcher (int sig)
{
  /* do nothing */
}


/**
 * setup handlers to ignore SIGPIPE.
 */
#ifndef MINGW
static void
ignore_sigpipe ()
{
  struct sigaction oldsig;
  struct sigaction sig;

  sig.sa_handler = &catcher;
  sigemptyset (&sig.sa_mask);
#ifdef SA_INTERRUPT
  sig.sa_flags = SA_INTERRUPT;  /* SunOS */
#else
  sig.sa_flags = SA_RESTART;
#endif
  if (0 != sigaction (SIGPIPE, &sig, &oldsig))
    fprintf (stderr,
             "Failed to install SIGPIPE handler: %s\n", strerror (errno));
}
#endif


/**
 * Entry point to demo.  Note: this HTTP server will make all
 * files in the current directory and its subdirectories available
 * to anyone.  Press ENTER to stop the server once it has started.
 *
 * @param argc number of arguments in argv
 * @param argv first and only argument should be the port number
 * @return 0 on success
 */
int
main (int argc, char *const *argv)
{
  struct MHD_Daemon *d;
  unsigned int port;

  if ( (argc != 2) ||
       (1 != sscanf (argv[1], "%u", &port)) ||
       (UINT16_MAX < port) )
    {
      fprintf (stderr,
	       "%s PORT\n", argv[0]);
      return 1;
    }
  #ifndef MINGW
  ignore_sigpipe ();
  #endif
  magic = magic_open (MAGIC_MIME_TYPE);
  (void) magic_load (magic, NULL);

  (void) pthread_mutex_init (&mutex, NULL);
  file_not_found_response = MHD_create_response_from_buffer (strlen (FILE_NOT_FOUND_PAGE),
							     (void *) FILE_NOT_FOUND_PAGE,
							     MHD_RESPMEM_PERSISTENT);
  mark_as_html (file_not_found_response);
  request_refused_response = MHD_create_response_from_buffer (strlen (REQUEST_REFUSED_PAGE),
							     (void *) REQUEST_REFUSED_PAGE,
							     MHD_RESPMEM_PERSISTENT);
  mark_as_html (request_refused_response);
  internal_error_response = MHD_create_response_from_buffer (strlen (INTERNAL_ERROR_PAGE),
							     (void *) INTERNAL_ERROR_PAGE,
							     MHD_RESPMEM_PERSISTENT);
  mark_as_html (internal_error_response);
  d = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG
#if EPOLL_SUPPORT
			| MHD_USE_EPOLL_LINUX_ONLY
#endif
			,
                        port,
                        NULL, NULL,
			&generate_page, NULL,
			MHD_OPTION_CONNECTION_MEMORY_LIMIT, (size_t) (256 * 1024),
#if PRODUCTION
			MHD_OPTION_PER_IP_CONNECTION_LIMIT, (unsigned int) (64),
#endif
			MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) (120 /* seconds */),
			MHD_OPTION_THREAD_POOL_SIZE, (unsigned int) NUMBER_OF_THREADS,
			MHD_OPTION_NOTIFY_COMPLETED, &response_completed_callback, NULL,
			MHD_OPTION_END);
  if (NULL == d)
    return 1;
  fprintf (stderr, "HTTP server running. Press ENTER to stop the server\n");
  (void) getc (stdin);
  MHD_stop_daemon (d);
  MHD_destroy_response (file_not_found_response);
  MHD_destroy_response (request_refused_response);
  MHD_destroy_response (internal_error_response);
  update_cached_response (NULL);
  (void) pthread_mutex_destroy (&mutex);
  magic_close (magic);
  return 0;
}

/* end of demo.c */
