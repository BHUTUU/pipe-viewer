#if defined(_WIN32) || defined(_WIN64)
#include <stdio.h>
#include <stdlib.h>

ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    size_t pos = 0;
    int c;

    if (*lineptr == NULL || *n == 0) {
        *n = 128;
        *lineptr = malloc(*n);
        if (*lineptr == NULL) return -1;
    }

    while ((c = fgetc(stream)) != EOF) {
        if (pos + 1 >= *n) {
            *n *= 2;
            char *new_ptr = realloc(*lineptr, *n);
            if (!new_ptr) return -1;
            *lineptr = new_ptr;
        }
        (*lineptr)[pos++] = c;
        if (c == '\n') break;
    }

    if (pos == 0 && c == EOF) return -1;
    (*lineptr)[pos] = '\0';
    return pos;
}
#endif
// #include <stdio.h>
// #include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <getopt.h>

#define VERSION "1.1"
#define DEFAULT_RATE_LIMIT 0  // 0 means no limit

// Function declarations
void print_usage(const char *progname);
void print_version(void);
void print_stats(void);
void process_line(const char *line);
void process_input(FILE *input);

struct {
  int quiet;               // -q flag
  unsigned long rate_limit; // -L bytes per second
  size_t total_bytes;
  time_t start_time;
} config;

void print_usage(const char *progname) {
  fprintf(stderr, "Usage: %s [-q] [-L RATE] [FILE]...\n", progname);
  fprintf(stderr, "Line-by-line pipe viewer with rate limiting\n\n");
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "  -q         Quiet mode, no statistics\n");
  fprintf(stderr, "  -L RATE    Limit transfer to RATE bytes per second\n");
  fprintf(stderr, "  --help     Display this help and exit\n");
  fprintf(stderr, "  --version  Output version information and exit\n");
}

void print_version(void) {
  printf("pv-like %s\n", VERSION);
}

void print_stats(void) {
  if (config.quiet) return;
  time_t now = time(NULL);
  double elapsed = difftime(now, config.start_time);
  double rate = elapsed > 0 ? config.total_bytes / elapsed : 0;
  fprintf(stderr, "\r%zu bytes (%6.1f KB/s)", config.total_bytes, rate / 1024);
  fflush(stderr);
}

void process_line(const char *line) {
  size_t len = strlen(line);
  config.total_bytes += len;

  // Apply rate limiting
  if (config.rate_limit > 0) {
    static size_t bytes_this_second = 0;
    static time_t current_second = 0;

    time_t now = time(NULL);
    if (now != current_second) {
      current_second = now;
      bytes_this_second = 0;
    }

    bytes_this_second += len;
    if (bytes_this_second > config.rate_limit) {
      // Calculate how much we're over the limit
      double over = bytes_this_second - config.rate_limit;
      // Sleep for the appropriate time (in microseconds)
      usleep(over * 1000000 / config.rate_limit);
    }
  }

  // Output the line immediately
  fputs(line, stdout);
  fflush(stdout);

  // Update stats
  if (!config.quiet) {
    print_stats();
  }
}

void process_input(FILE *input) {
  char *line = NULL;
  size_t len = 0;
  ssize_t read;

  config.start_time = time(NULL);

  while ((read = getline(&line, &len, input)) != -1) {
    process_line(line);
  }

  free(line);
  if (!config.quiet) fprintf(stderr, "\n");
}

int main(int argc, char *argv[]) {
  // Initialize config
  config.quiet = 0;
  config.rate_limit = DEFAULT_RATE_LIMIT;
  config.total_bytes = 0;

  // Parse command line options
  static struct option long_options[] = {
    {"quiet", no_argument, 0, 'q'},
    {"rate-limit", required_argument, 0, 'L'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'v'},
    {0, 0, 0, 0}
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "qL:hv", long_options, NULL)) != -1) {
    switch (opt) {
      case 'q':
        config.quiet = 1;
          break;
      case 'L':
        config.rate_limit = atol(optarg);
        break;
      case 'h':
        print_usage(argv[0]);
        return 0;
      case 'v':
        print_version();
        return 0;
      default:
        print_usage(argv[0]);
        return 1;
    }
  }

  // Handle input
  if (optind < argc) {
    for (int i = optind; i < argc; i++) {
      FILE *file = fopen(argv[i], "r");
      if (!file) {
        perror(argv[i]);
        continue;
      }
      process_input(file);
      fclose(file);
    }
  } else {
    if (isatty(STDIN_FILENO) && !config.quiet) {
      fprintf(stderr, "Waiting for line input... (use pipe or specify files)\n");
    }
    process_input(stdin);
  }

  return 0;
}
