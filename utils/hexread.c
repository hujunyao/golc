#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

const char* hextable[] = {
  "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL", "BS", "HT", "LF", "VT", "FF", "CR", "SO", "SI",
  "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB", "CAN", "EM", "SUB", "ESC", "FS", "GS", "RS", "US"
};
int main(int argc, char* argv[]) {
  FILE* fp = NULL;
  char* ptr = NULL, *contents = NULL;
  int nread = 0, nbytes = 0, is_eof = 3;

  fp = fopen(argv[1], "r");
  if(fp != NULL) {
    struct stat fi;
    fstat(fileno(fp), &fi);
    ptr = contents = calloc(fi.st_size, sizeof(char));
    nread = fread(contents, sizeof(char), fi.st_size, fp);
    fclose(fp);

    while(nbytes < nread && is_eof) {
      if(*ptr >=0 && *ptr <= 0x1F) {
        printf("%s", hextable[*ptr]);
        if(*ptr == 0)
          is_eof--;
        else
          is_eof = 3;
      } else if(*ptr >= 0x20 && *ptr < 0x7F){
        printf("%c", *ptr);
      } else {
        printf("%02X", (0xFF&*ptr));
      }
      nbytes++;
      ptr++;
    }
    printf("\r\n");
    printf("%s: filesize = %lld, nread=%d, nbytes=%d\n", argv[1], fi.st_size, nread, nbytes);
    free(contents);
  }
}
