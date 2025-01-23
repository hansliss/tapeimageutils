#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include "tapeio.h"

#define RECSIZE (15*518*5)

void usage(char *progname) {
  fprintf(stderr, "Usage: %s -f <filename> <infile> [<infile> ...]\n", progname);
}

int main(int argc, char *argv[]) {
  int o;
  char *filename = NULL;
  struct stat statbuf;
  unsigned char *filebuf;
  // Get the filename parameter
  while ((o = getopt(argc, argv, "f:")) != -1) {
    switch (o) {
    case 'f':
      filename = optarg;
      break;
    default:
      usage(argv[0]);
      return -1;
    }
  }
  if (!filename) {
    usage(argv[0]);
    return -2;
  }

  // Create a new tape image
  tape_handle_t tape = opentape(filename, 1, 1);
  // Loop over the filenames on the command line
  for (;optind < argc; optind++) {

    // Find the size of the current file
    stat(argv[optind], &statbuf);
    long size = statbuf.st_size;

    // Open the file and map it into memoty
    int f = open(argv[optind], O_RDONLY);
    if (f == -1) {
      perror(argv[optind]);
      return -9;
    }
    fprintf(stderr, "File %s, length %ld\n", argv[optind], size);
    if ((filebuf = mmap(NULL, size, PROT_READ, MAP_PRIVATE, f, 0)) == MAP_FAILED) {
      perror("mmap()");
      return -10;
    }

    // Now we can close the file
    close(f);

    if ((size % 518) * 5 == 0) {
      // Write the file to the tape image
      for (int i=0; i<size; i += RECSIZE) {
	int len = (size - i < RECSIZE)?(size - i): RECSIZE;
	// fprintf(stderr, "Reclen: %d\n", len);
	putrec(tape, &(filebuf[i]), len);
      }
    } else if (size % 80 == 0) {
      for (int i=0; i<size; i+=80) {
	putrec(tape, &(filebuf[i]), 80);
      }
    } else {
      fprintf(stderr, "Unknown data length %ld\n", size);
      return -11;
    }

    // Unmap
    munmap(filebuf, size);
  }
  // Write a tape mark and close the tape image
  tapemark(tape);
  closetape(tape);
  return 0;
}
