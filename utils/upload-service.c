/** compile command: gcc -o upload.service upload-service.c `pkg-config libevent libmicrohttpd --cflags --libs` -lmagic -lepeg
 ** curl -F "from_jid=wuruxu" -F "to_jid=admin" -F "upload=@IMG_20130212_123414.jpg" http://127.0.0.1:8080/upload
*/
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
#include <event.h>
#include <dirent.h>

#include "Epeg.h"

#define DD(...) fprintf(stderr, ":[D] "__VA_ARGS__)

#define THUMBNAIL_MAX_DIMENSION 196

#define NUMBER_OF_THREADS 6
#define MAGIC_HEADER_SIZE (16 * 1024)

#define FILE_NOT_FOUND_PAGE "<html><head><title>File not found</title></head><body>File not found</body></html>"

#define INTERNAL_ERROR_PAGE "<html><head><title>Internal error</title></head><body>HTTP Server Internal error</body></html>"

//#define REQUEST_REFUSED_PAGE "<html><head><title>Request refused</title></head><body>Request refused (file exists?)</body></html>"
#define REQUEST_REFUSED_PAGE "{'status': 'REFUSED', 'action': 'upload'}"

#define UPLOAD_OK_JSON "{'status': 'OK', 'action': 'upload'}"

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


static struct MHD_Response *upload_ok_json_response;
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
static void mark_as_html (struct MHD_Response *response) {
  MHD_add_response_header (response, MHD_HTTP_HEADER_CONTENT_TYPE, "text/html");
}

static void mark_as_json(struct MHD_Response *response) {
  MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "application/json");
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
struct ResponseDataContext {
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
struct UploadContext {
  /**
   * Handle where we write the uploaded file to.
   */
  int fd;

  /**
   * Name of the file on disk (used to remove on errors).
   */
  char *filename;
  char *thumbnail;

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
static int do_append (char **ret, const char *data, size_t size) {
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
  int i, writen = 0;

  //fprintf(stderr, "key = %s, filename = %s\n", key, filename);
  if (0 == strcmp(key, "from_jid"))
    return do_append (&uc->from_jid, data, size);
  if (0 == strcmp(key, "to_jid"))
    return do_append (&uc->to_jid, data, size);
  if (0 != strcmp (key, "upload")) {
      DD("Ignoring unexpected form value `%s'\n", key);
      return MHD_YES; /* ignore */
    }

  if (NULL == filename) {
    DD("No filename, aborting upload\n");
    return MHD_NO; /* no filename, error */
  }

  if ( (NULL == uc->to_jid) || (NULL == uc->from_jid) ) {
    DD("Missing form data for upload `%s'\n", filename);
    uc->response = request_refused_response;
    return MHD_NO;
  }

  if (-1 == uc->fd) {
    char fn[PATH_MAX] = {0};
    char fn_thumb[PATH_MAX] = {0};

    if ( (NULL != strstr (filename, "..")) || (NULL != strchr (filename, '/')) || (NULL != strchr (filename, '\\')) ) {
      uc->response = request_refused_response;
      return MHD_NO;
    }

    /* create directories -- if they don't exist already */
    (void) mkdir (uc->to_jid, S_IRWXU);
    snprintf (fn, sizeof (fn), "%s/%s", uc->to_jid, uc->from_jid);

    (void) mkdir (fn, S_IRWXU);

    /**create thumbnail directory*/
    snprintf(fn_thumb, sizeof(fn_thumb), "%s/%s/.thumb", uc->to_jid, uc->from_jid);
    (void) mkdir(fn_thumb, S_IRWXU);
    snprintf(fn_thumb, sizeof(fn_thumb), "%s/%s/.thumb/%s", uc->to_jid, uc->from_jid, filename);

    snprintf (fn, sizeof (fn), "%s/%s/%s", uc->to_jid, uc->from_jid, filename);
    for (i=strlen (fn)-1;i>=0;i--)
      if (! isprint ((int) fn[i]))
        fn[i] = '_';

    uc->fd = open (fn, O_CREAT | O_EXCL | O_WRONLY, S_IRUSR | S_IWUSR);
    if (-1 == uc->fd) {
      DD( "Error opening file `%s' for upload: %s\n", fn, strerror (errno));
      uc->response = request_refused_response;
      return MHD_NO;
    }
    uc->filename = strdup (fn);
    uc->thumbnail = strdup (fn_thumb);
  }

  if(size > 0)
    writen = write(uc->fd, data, size);

  //DD("write data to file, data size = %d, writen size = %d\n", size, writen);

  if ( (0 != size) && (size != writen) ) {
    /* write failed; likely: disk full */
    DD("Error writing to file `%s': %s\n", uc->filename, strerror (errno));
    uc->response = internal_error_response;
    close (uc->fd);
    uc->fd = -1;

    if (NULL != uc->filename) {
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
  if (NULL != uc->thumbnail)
    free (uc->thumbnail);

  free (uc);
}


/**
 * Return the current directory listing.
 *
 * @param connection connection to return the directory for
 * @return MHD_YES on success, MHD_NO on error
 */
static int
return_directory_response (struct MHD_Connection *connection) {
  int ret;

  (void) pthread_mutex_lock (&mutex);
  if (NULL == cached_directory_response)
    ret = MHD_queue_response (connection, MHD_HTTP_INTERNAL_SERVER_ERROR, internal_error_response);
  else
    ret = MHD_queue_response (connection, MHD_HTTP_OK, cached_directory_response);
  (void) pthread_mutex_unlock (&mutex);
  return ret;
}

const char *get_mimetype(char *filename) {
  char file_data[MAGIC_HEADER_SIZE] = {0};
  ssize_t got;
  int fd;
  const char *mime = NULL;
  
  fd = open(filename, O_RDONLY);
  if(-1 == fd) {
    return mime;
  }

  got = read(fd, file_data, sizeof(file_data));

  if (-1 != got) {
    mime = magic_buffer(magic, file_data, got);
  }

  if(fd != -1)
    close(fd);

  return mime;
}

void generate_video_thumbnail(char *filename, char *thumbnail) {
  char cmd[PATH_MAX] = {0};

  snprintf(cmd, PATH_MAX, "ffmpegthumbnailer -c jpeg -i %s -s %d -q 10 -o %s", filename, THUMBNAIL_MAX_DIMENSION, thumbnail);
  DD("generate_video_thumbnail cmd = %s\n", cmd);
  system(cmd);
}

void generate_jpeg_thumbnail(char *filename, char *thumbnail) {
  Epeg_Image *im = NULL;
  Epeg_Thumbnail_Info info;
  int w,h, thumb_width, thumb_height;

  im = epeg_file_open(filename);
  if(! im) {
    DD("epeg library open failed\n");
    return;
  }
  
  epeg_size_get(im, &w, &h);
  if(w > h) {
    thumb_width = THUMBNAIL_MAX_DIMENSION;
    thumb_height = THUMBNAIL_MAX_DIMENSION * h/w;
  } else {
    thumb_height = THUMBNAIL_MAX_DIMENSION;
    thumb_width = THUMBNAIL_MAX_DIMENSION * w /h;
  }

  epeg_decode_size_set(im, thumb_width, thumb_height);
  epeg_quality_set               (im, 100);
  epeg_thumbnail_comments_enable (im, 0);
  epeg_file_output_set           (im, thumbnail);
  epeg_encode                    (im);
  epeg_close                     (im);
}

void generate_thumbnail(char *input, char *output) {
  const char *mimetype = get_mimetype(input);
  DD("mime =  %s\n", get_mimetype(input));
  if(strcmp(mimetype, "image/jpeg") == 0) {
    generate_jpeg_thumbnail(input, output);
  } else if(strcmp(mimetype, "video/mp4") == 0) {
    generate_video_thumbnail(input, output);
  }
}

int list_media_files(char *fn, FILE *fp) {
  DIR *dp = NULL;
  struct dirent *dent = NULL;
  int cnt = 0;
  char xmlbuf[1024] = {0};

  dp = opendir(fn);
  while((dent = readdir(dp)) != NULL) {
    char newfn[PATH_MAX] = {0};
    char thumbpath[1024] = {0};
    struct stat info;

    if(dent->d_name[0] == '.')
      continue;
    snprintf(newfn, PATH_MAX, "%s/%s", fn, dent->d_name);
    snprintf(thumbpath, PATH_MAX, "%s/.thumb/%s", fn, dent->d_name);
    stat(newfn, &info);
    if(S_ISDIR(info.st_mode)) {
      continue;
    }
    //DD("fn: %s\n", newfn);
    snprintf(xmlbuf, 1024, "<item><thumb>%s</thumb><data>%s</data></item>", thumbpath, newfn);
    fwrite(xmlbuf, 1, strlen(xmlbuf), fp);
    cnt ++;
  }
  closedir(dp);
  //DD("total media size %d\n", cnt);
  return cnt;
}

#define XML_HEADER  "<?xml version='1.0' encoding='UTF-8'?>"
#define XML_HEADER_LEN (size_t)strlen(XML_HEADER)

#define XML_MEDIAINFO_START "<mediainfo>"
#define XML_MEDIAINFO_START_LEN (size_t)strlen(XML_MEDIAINFO_START)
#define XML_MEDIAINFO_END "</mediainfo>"
#define XML_MEDIAINFO_END_LEN (size_t)strlen(XML_MEDIAINFO_END)

void generate_mediainfo_by_jid(const char *jid) {
  DIR *dp = NULL;
  struct dirent *dent = NULL;
  int cnt = 0, fd = 0;
  char fn[PATH_MAX] = {0};
  FILE *fp = NULL;

  dp = opendir(jid);

  snprintf(fn, PATH_MAX, "%s/index.xml", jid);
  fp = fopen(fn, "w");
  if(fp == NULL) {
    DD("generate %s failure\n", fn);
  }

  fwrite(XML_HEADER, 1, XML_HEADER_LEN, fp);
  fwrite(XML_MEDIAINFO_START, 1, XML_MEDIAINFO_START_LEN, fp);
  while((dent = readdir(dp)) != NULL) {
    struct stat info;
    char fn[PATH_MAX] = {0};

    if(dent->d_name[0] == '.')
      continue;

    //DD("d_name = %s", dent->d_name);
    snprintf(fn, PATH_MAX, "%s/%s", jid, dent->d_name);
    stat(fn, &info);
    if(S_ISDIR(info.st_mode)) {
      //DD(" is directory\n");
      cnt += list_media_files(fn, fp);
    } else {
      //DD(" is file\n");
      continue;
    }
  }
  fwrite(XML_MEDIAINFO_END, 1, XML_MEDIAINFO_END_LEN, fp);
  //DD("total media size %d\n", cnt);

  closedir(dp);
  fclose(fp);
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
static int generate_page (void *cls, struct MHD_Connection *connection,
	       const char *url, const char *method, const char *version,
	       const char *upload_data, size_t *upload_data_size, void **ptr) {
  struct MHD_Response *response;
  int ret, fd;
  struct stat buf;

  //DD("url = %s, method = %s\n", url, method);
  if (0 == strcmp (method, MHD_HTTP_METHOD_GET)) {
    if(0 == strncmp(url, "/list/", 6)) {
      char xmlfile[PATH_MAX] = {0};

      generate_mediainfo_by_jid(url+6);
      snprintf(xmlfile, PATH_MAX, "%s/%s.xml", url+6, url+6);
      fd = open(xmlfile, O_RDONLY);

      stat(xmlfile, &buf);
      if(-1 == fd)
        return MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, file_not_found_response);


      response = MHD_create_response_from_fd(buf.st_size, fd);
       if(NULL == response) {
         close(fd);
         return MHD_NO;
       }
       MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, "application/xml");

       ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
       MHD_destroy_response(response);
       return ret;
    }

    if(0 != strcmp(url, "/")) {
      char file_data[MAGIC_HEADER_SIZE];
      ssize_t got;
      const char *mime = NULL;

      if(0 == stat(url+1, &buf) && (NULL == strstr(url+1, "..")) && ('/' != url[1]))
        fd = open(url+1, O_RDONLY);
      else
        fd = -1;

      if(-1 == fd)
        return MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, file_not_found_response);

      got = read(fd, file_data, sizeof(file_data));
      if(-1 != got)
        mime = magic_buffer(magic, file_data, got);
      else
        mime = NULL;

       lseek(fd, 0, SEEK_SET);
       response = MHD_create_response_from_fd(buf.st_size, fd);
       if(NULL == response) {
         close(fd);
         return MHD_NO;
       }
       if(NULL != mime)
         MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, mime);

       ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
       MHD_destroy_response(response);
       return ret;
    }
  } /**MHD_HTTP_METHOD_GET*/

  if (0 == strcmp (method, MHD_HTTP_METHOD_POST)) {
    struct UploadContext *uc = *ptr;

    if (NULL == uc) {
      if (NULL == (uc = malloc (sizeof (struct UploadContext))))
        return MHD_NO; /* out of memory, close connection */

      memset (uc, 0, sizeof (struct UploadContext));
      uc->fd = -1;
      uc->connection = connection;

      uc->pp = MHD_create_post_processor (connection, 64 * 1024 /* buffer size */, &process_upload_data, uc);
      if (NULL == uc->pp) {
        free (uc);
        return MHD_NO;
      }

      *ptr = uc;
      return MHD_YES;
    }

    if (0 != *upload_data_size) {
      if (NULL == uc->response)
        (void) MHD_post_process (uc->pp, upload_data, *upload_data_size);
        *upload_data_size = 0;
        return MHD_YES;
    }

    /* end of upload, finish it! */
    MHD_destroy_post_processor (uc->pp);
    uc->pp = NULL;

    if (-1 != uc->fd) {
      close (uc->fd);
      uc->fd = -1;
    }

    /**generate jpeg thumbnail use epeg library*/
    generate_thumbnail(uc->filename, uc->thumbnail);

    if (NULL != uc->response) {
      return MHD_queue_response (connection, MHD_HTTP_FORBIDDEN, uc->response);
    } else {
      return MHD_queue_response(connection, MHD_HTTP_OK, upload_ok_json_response);
    }
  }/**MHD_HTTP_METHOD_POST*/

  if (0 == strcmp (method, MHD_HTTP_METHOD_GET)) {
    return return_directory_response (connection);
  }

  /* unexpected method request, refuse */
  return MHD_queue_response (connection, MHD_HTTP_FORBIDDEN, request_refused_response);
}

static void signal_cb(evutil_socket_t sig, short events, void *data) {
  struct event_base *base = data;
  struct timeval delay = { 0, 0 };

  DD("upload-service quit now\n");
  event_base_loopexit(base, &delay);
}

/**
 * Entry point to demo.  Note: this HTTP server will make all
 * files in the current directory and its subdirectories available
 * to anyone.  Press ENTER to stop the server once it has started.
 *
 * @param argc number of arguments in argv
 * @param argv first and only argument should be the port number
 * @return 0 on success
 */
int main (int argc, char *const *argv) {
  struct MHD_Daemon *d;
  unsigned int port;
  struct event_base *base = NULL;
  struct event *signal_event = NULL;

  if ( (argc != 2) || (1 != sscanf (argv[1], "%u", &port)) || (UINT16_MAX < port) ) {
      fprintf (stderr, "%s PORT\n", argv[0]);
      return 1;
    }

  generate_mediainfo_by_jid("admin");
  return 0;

  base = event_base_new();
  magic = magic_open (MAGIC_MIME_TYPE);
  (void) magic_load (magic, NULL);
  (void) pthread_mutex_init (&mutex, NULL);

  file_not_found_response = MHD_create_response_from_buffer (strlen (FILE_NOT_FOUND_PAGE), (void *) FILE_NOT_FOUND_PAGE, MHD_RESPMEM_PERSISTENT);
  mark_as_html (file_not_found_response);

  request_refused_response = MHD_create_response_from_buffer (strlen (REQUEST_REFUSED_PAGE), (void *) REQUEST_REFUSED_PAGE, MHD_RESPMEM_PERSISTENT);
  mark_as_html (request_refused_response);

  upload_ok_json_response = MHD_create_response_from_buffer (strlen(UPLOAD_OK_JSON), (void *) UPLOAD_OK_JSON, MHD_RESPMEM_PERSISTENT);
  mark_as_json(upload_ok_json_response);

  internal_error_response = MHD_create_response_from_buffer (strlen (INTERNAL_ERROR_PAGE), (void *) INTERNAL_ERROR_PAGE, MHD_RESPMEM_PERSISTENT);
  mark_as_html (internal_error_response);

  d = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY | MHD_USE_DEBUG | MHD_USE_EPOLL_LINUX_ONLY
			, port, NULL, NULL, &generate_page, NULL,
			MHD_OPTION_CONNECTION_MEMORY_LIMIT, (size_t) (256 * 1024),
			MHD_OPTION_PER_IP_CONNECTION_LIMIT, (unsigned int) (64),
			MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) (120 /* seconds */),
			MHD_OPTION_THREAD_POOL_SIZE, (unsigned int) NUMBER_OF_THREADS,
			MHD_OPTION_NOTIFY_COMPLETED, &response_completed_callback, NULL,
			MHD_OPTION_END);
  if (NULL == d)
    return 1;

  signal_event = evsignal_new(base, SIGINT, signal_cb, (void *)base);
  event_add(signal_event, NULL);
  event_base_dispatch(base);

  event_base_free(base);
  MHD_stop_daemon (d);
  MHD_destroy_response (file_not_found_response);
  MHD_destroy_response (request_refused_response);
  MHD_destroy_response (upload_ok_json_response);
  MHD_destroy_response (internal_error_response);
  update_cached_response (NULL);
  (void) pthread_mutex_destroy (&mutex);
  magic_close (magic);

  return 0;
}
