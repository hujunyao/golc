/** cgroups FastCGI demo
 ** compile command: gcc -g -o fcgi-bin/net.fastcgi net-fastcgi.c -lmaxminddb  -lfcgi
 ** start fastcgi: spawn-fcgi -n -a 127.0.0.1 -p 9000 /home/user/fcgi-bin/net.fastcgi
 ** I have tested with lighttpd on my linux PC
 **/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#define FCGI_DEBUG 0

#if(FCGI_DEBUG)
#include "maxminddb.h"
char **environ = NULL;
#define DEBUG(...) fprintf(stderr, __VA_ARGS__)
#else
extern char **environ;
#include <fcgi_stdio.h>
#include "maxminddb.h"
#define DEBUG
#endif

#define OUT printf
#define URIMAX 256

static void dumpEnv(char **envp) {
  OUT("<EnvDump>");
  for(; *envp != NULL; envp++) {
    OUT("<env>%s</env>", *envp);
  }
  OUT("</EnvDump>\n");
}

static char *
mmdb_strdup(const char *str)
{
	size_t len;
	char *copy;

	len = strlen(str) + 1;
	if ((copy = malloc(len)) == NULL)
		return (NULL);
	memcpy(copy, str, len);
	return (copy);
}

static size_t
mmdb_strnlen(const char *s, size_t maxlen)
{
    size_t len;

    for (len = 0; len < maxlen; len++, s++) {
        if (!*s)
            break;
    }
    return (len);
}

static char *
mmdb_strndup(const char *str, size_t n)
{
	size_t len;
	char *copy;

	len = mmdb_strnlen(str, n);
	if ((copy = malloc(len + 1)) == NULL)
		return (NULL);
	memcpy(copy, str, len);
	copy[len] = '\0';
	return (copy);
}

static int print_indentation(char *buffer, int i) {
  memset(buffer, 32, i);
  return i;
}

static char *bytes_to_hex(uint8_t *bytes, uint32_t size)
{
    char *hex_string = malloc((size * 2) + 1);
    char *hex_pointer = hex_string;
	uint32_t i = 0;

    for (; i < size; i++) {
        sprintf(hex_pointer + (2 * i), "%02X", bytes[i]);
    }

    return hex_string;
}

static MMDB_entry_data_list_s *buffer_entry_data_list(char *buffer, MMDB_entry_data_list_s *entry_data_list, int indent, int *status, int *len) {
	char *ptr = buffer;
	int bufsize = 0;
    switch (entry_data_list->entry_data.type) {
    case MMDB_DATA_TYPE_MAP:
        {
            uint32_t size = entry_data_list->entry_data.data_size;

            ptr = ptr + print_indentation(ptr, indent);
            //*ptr='{', *(ptr+1) = '\n', ptr += 2;
			ptr = ptr + sprintf(ptr, "{\n");
            indent += 2;

            for (entry_data_list = entry_data_list->next;
                 size && entry_data_list; size--) {

                char *key =
                    mmdb_strndup(
                        (char *)entry_data_list->entry_data.utf8_string,
                        entry_data_list->entry_data.data_size);
                if (NULL == key) {
                    *status = MMDB_OUT_OF_MEMORY_ERROR;
                    return NULL;
                }

                ptr = ptr + print_indentation(ptr, indent);
				ptr = ptr + sprintf(ptr, "\"%s\": \n", key);
                //fprintf(stream, "\"%s\": \n", key);
                free(key);

                entry_data_list = entry_data_list->next;
                entry_data_list = buffer_entry_data_list(ptr, entry_data_list, indent + 2, status, &bufsize);
				ptr = ptr + bufsize;

                if (MMDB_SUCCESS != *status) {
                    return NULL;
                }
            }

            indent -= 2;
            //print_indentation(stream, indent);
			ptr = ptr + print_indentation(ptr, indent);
			ptr = ptr + sprintf(ptr, "}\n");
            //fprintf(stream, "}\n");
        }
        break;
    case MMDB_DATA_TYPE_ARRAY:
        {
            uint32_t size = entry_data_list->entry_data.data_size;

            //print_indentation(stream, indent);
			ptr = ptr + print_indentation(ptr, indent);
			ptr = ptr + sprintf(ptr, "[\n");
            //fprintf(stream, "[\n");
            indent += 2;

            for (entry_data_list = entry_data_list->next;
                 size && entry_data_list; size--) {
                entry_data_list = buffer_entry_data_list(ptr, entry_data_list, indent, status, &bufsize);
				ptr = ptr + bufsize;
                if (MMDB_SUCCESS != *status) {
                    return NULL;
                }
            }

            indent -= 2;
            //print_indentation(stream, indent);
			ptr = ptr + print_indentation(ptr, indent);
            //fprintf(stream, "]\n");
			ptr = ptr + sprintf(ptr, "]\n");
        }
        break;
    case MMDB_DATA_TYPE_UTF8_STRING:
        {
            char *string =
                mmdb_strndup((char *)entry_data_list->entry_data.utf8_string,
                             entry_data_list->entry_data.data_size);
            if (NULL == string) {
                *status = MMDB_OUT_OF_MEMORY_ERROR;
                return NULL;
            }
            //print_indentation(stream, indent);
			ptr = ptr + print_indentation(ptr, indent);
            //fprintf(stream, "\"%s\" <utf8_string>\n", string);
			ptr = ptr + sprintf(ptr, "\"%s\" <utf8_string>\n", string);
            free(string);
            entry_data_list = entry_data_list->next;
        }
        break;
    case MMDB_DATA_TYPE_BYTES:
        {
            char *hex_string =
                bytes_to_hex((uint8_t *)entry_data_list->entry_data.bytes,
                             entry_data_list->entry_data.data_size);
            if (NULL == hex_string) {
                *status = MMDB_OUT_OF_MEMORY_ERROR;
                return NULL;
            }

            //print_indentation(stream, indent);
			ptr = ptr + print_indentation(ptr, indent);
            //fprintf(stream, "%s <bytes>\n", hex_string);
			ptr = ptr + sprintf(ptr, "%s <bytes>\n", hex_string);
            free(hex_string);

            entry_data_list = entry_data_list->next;
        }
        break;
    case MMDB_DATA_TYPE_DOUBLE:
        //print_indentation(stream, indent);
		ptr = ptr + print_indentation(ptr, indent);
        //fprintf(stream, "%f <double>\n",
		ptr = ptr + sprintf(ptr, "%f <double>\n",
                entry_data_list->entry_data.double_value);
        entry_data_list = entry_data_list->next;
        break;
    case MMDB_DATA_TYPE_FLOAT:
        //print_indentation(stream, indent);
		ptr = ptr + print_indentation(ptr, indent);
        //fprintf(stream, "%f <float>\n",
		ptr = ptr + sprintf(ptr, "%f <float>\n",
                entry_data_list->entry_data.float_value);
        entry_data_list = entry_data_list->next;
        break;
    case MMDB_DATA_TYPE_UINT16:
        //print_indentation(stream, indent);
		ptr = ptr + print_indentation(ptr, indent);
		ptr = ptr + sprintf(ptr, "%u <uint16>\n", entry_data_list->entry_data.uint16);
        //fprintf(stream, "%u <uint16>\n", entry_data_list->entry_data.uint16);
        entry_data_list = entry_data_list->next;
        break;
    case MMDB_DATA_TYPE_UINT32:
        //print_indentation(stream, indent);
		ptr = ptr + print_indentation(ptr, indent);
        //fprintf(stream, "%u <uint32>\n", entry_data_list->entry_data.uint32);
		ptr = ptr + sprintf(ptr, "%u <uint32>\n", entry_data_list->entry_data.uint32);
        entry_data_list = entry_data_list->next;
        break;
    case MMDB_DATA_TYPE_BOOLEAN:
        //print_indentation(stream, indent);
		ptr = ptr + print_indentation(ptr, indent);
        //fprintf(stream, "%s <boolean>\n",
		ptr = ptr + sprintf(ptr, "%s <boolean>\n",
                entry_data_list->entry_data.boolean ? "true" : "false");
        entry_data_list = entry_data_list->next;
        break;
    case MMDB_DATA_TYPE_UINT64:
        //print_indentation(stream, indent);
		ptr = ptr + print_indentation(ptr, indent);
        //fprintf(stream, "%" PRIu64 " <uint64>\n",
		ptr = ptr + sprintf(ptr, "%" PRIu64 " <uint64>\n",
                entry_data_list->entry_data.uint64);
        entry_data_list = entry_data_list->next;
        break;
    case MMDB_DATA_TYPE_UINT128:
        //print_indentation(stream, indent);
		ptr = ptr + print_indentation(ptr, indent);
#if MMDB_UINT128_IS_BYTE_ARRAY
        char *hex_string =
            bytes_to_hex((uint8_t *)entry_data_list->entry_data.uint128, 16);
        //fprintf(stream, "0x%s <uint128>\n", hex_string);
		ptr = ptr + sprintf(ptr,  "0x%s <uint128>\n", hex_string);
        free(hex_string);
#else
        uint64_t high = entry_data_list->entry_data.uint128 >> 64;
        uint64_t low = (uint64_t)entry_data_list->entry_data.uint128;
		ptr = ptr + sprintf(ptr, "0x%016" PRIX64 "%016" PRIX64 " <uint128>\n", high,
        //fprintf(stream, "0x%016" PRIX64 "%016" PRIX64 " <uint128>\n", high,
                low);
#endif
        entry_data_list = entry_data_list->next;
        break;
    case MMDB_DATA_TYPE_INT32:
        //print_indentation(stream, indent);
		ptr = ptr + print_indentation(ptr, indent);
        //fprintf(stream, "%d <int32>\n", entry_data_list->entry_data.int32);
		ptr = ptr + sprintf(ptr, "%d <int32>\n", entry_data_list->entry_data.int32);
        entry_data_list = entry_data_list->next;
        break;
    default:
        *status = MMDB_INVALID_DATA_ERROR;
        return NULL;
    }

    *status = MMDB_SUCCESS;
	*len = ptr - buffer;
    return entry_data_list;
}

static int MMDB_buffer_entry_data_list(char *buf, MMDB_entry_data_list_s *entry_data_list, int indent) {
  int status, len = 0;
  buffer_entry_data_list(buf, entry_data_list, indent, &status, &len);
  //DEBUG("MMDB_buffer_entry_data_list.len = %d\n", len);
  return status;
}

static int lookup_geoip(const char *ipaddr) {
  MMDB_s mmdb;
  int gai_error, mmdb_error;
  MMDB_lookup_result_s ret;
  MMDB_entry_data_list_s *entry_data_list = NULL;

  int status = MMDB_open("/home/user/www/data/GeoLite2-City.mmdb", MMDB_MODE_MMAP, &mmdb);
  if(MMDB_SUCCESS != status) {
    status = -128;
  }

  ret = MMDB_lookup_string(&mmdb, ipaddr,  &gai_error, &mmdb_error);
  if(MMDB_SUCCESS != mmdb_error) {
    status = -1;
  }

  if(ret.found_entry) {
    status = MMDB_get_entry_data_list(&ret.entry, &entry_data_list);
    if(MMDB_SUCCESS != status) {
      status = -2;
    }
    if(NULL != entry_data_list) {
      char jsonbuf[10240] = {0};
      MMDB_buffer_entry_data_list(jsonbuf, entry_data_list, 2);
      //OUT(jsonbuf);
      OUT("%s", jsonbuf);
      return 1;
    }
  } else {
    status = 0;
  }

  MMDB_free_entry_data_list(entry_data_list);
  MMDB_close(&mmdb);

  return status;
}

int main(int argc, char *argv[]) {
  char **reqEnv = environ;
  int err = 0;

#if(FCGI_DEBUG)
  if(1) {
#else
  while (FCGI_Accept() >= 0) {
#endif
    char *uri = getenv("REQUEST_URI");
    char *raddr = getenv("REMOTE_ADDR");
#if(FCGI_DEBUG)
#else
    if(! uri) continue;
#endif
    if(strcmp(uri, "/fcgi/geoip") == 0) {
      int status = 0;
      OUT("Content-type: application/json\r\n");
      OUT("\r\n");
      OUT("{\r\n\"address\": \"%s\"\r\n\"geoip\":\r\n", raddr);
      if(raddr != NULL) {
        status = lookup_geoip(raddr);
      } else {
        status = -127;
      }
      OUT("  \"status\": %d\r\n}", status);
    } else {
      OUT("Content-type: text/xml\r\n");
      OUT("\r\n");
      OUT("<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n");
      OUT("<root>");
	  OUT("<req>\r\n<uri>%s</uri>\r\n<ip>%s</ip>\r\n</req>\r\n", uri, getenv("REMOTE_ADDR"));
      dumpEnv(reqEnv);
      OUT("</root>");
    }
  }

  return 0;
}
