#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <iostream>
#include <iomanip>
#include <time.h>
#include <chrono>

#include "comb.h"

enum ListError {
  SKIP = -2,
  EOL,
};

enum Type { Lossy, Lossless };

typedef struct {
  uint64_t ori_size;
  uint64_t cmp_size;
  uint64_t cmp_time;
  uint64_t dec_time;
} Perf;

Perf empty = {0, 0, 0, 0};

/*********************************************************************
 *                      Evaluation Settings
 *********************************************************************/
// Available readers
int read_timestamp(FILE *file, double *data, int len) {
  char line[1024];
  int i = 0;
  while (fgets(line, 1024, file)) {
    char *token = strtok(line, ",");

    std::tm t = {};
    std::istringstream ss(token);
    ss >> std::get_time(&t, "%Y-%m-%dT%H:%M:%SZ");
    std::chrono::system_clock::time_point tp = std::chrono::system_clock::from_time_t(std::mktime(&t));
    std::chrono::duration<double> duration = tp.time_since_epoch();
    data[i++] = duration.count();
    if (i >= len)
      return i;
  }
  return i;
}

int read_float(FILE *file, double *data, int len) {
  char line[1024];
  int i = 0;
  while (fgets(line, 1024, file)) {
    strtok(line, ",");
    char *token = strtok(NULL, ",");
    data[i++] = atof(token);
    if (i >= len)
      return i;
  }
  return i;
}

int read_entire_file(FILE *file, double *data, int len) {
  int i = 0;
  char buffer[8];
  for (; i < len; ++i) {
    if (fread(buffer, sizeof(uint8_t), 8, file) != 8) {
      return i;
    }
    memcpy(&(data[i]), buffer, 8);
  }
  return i;
}

int (*readers[])(FILE *file, double *data, int len) = {read_float, read_timestamp, read_entire_file};

// Available compressors
char all_options[10][100] = {
  "gorilla,delta",
  "chimp,delta",
  "elf,delta,eraser",
  "gorilla,delta,eraser",
  "chimp,delta,eraser",
  "gorilla,timestamp",
  "chimp,timestamp,delta-of-delta"};

struct {
  char name[50];
  Type type;
  int (*compress)(double *in, size_t len, uint8_t **out, double error, const char *options);
  int (*decompress)(uint8_t *in, size_t len, double *out, double error, const char *options);
  Perf perf;
  const char *options;
} compressors[] = {
    {"Gorilla", Type::Lossless, comb_encode, comb_decode, empty, all_options[0]},
    {"Chimp", Type::Lossless, comb_encode, comb_decode, empty, all_options[1]},
    {"Elf", Type::Lossless, comb_encode, comb_decode, empty, all_options[2]},
    {"Gorilla+Eraser", Type::Lossless, comb_encode, comb_decode, empty, all_options[3]},
    {"Chimp+Eraser", Type::Lossless, comb_encode, comb_decode, empty, all_options[4]},
    {"Gorilla+Ts", Type::Lossless, comb_encode, comb_decode, empty, all_options[5]},
    {"Gorilla+Ts+DoD", Type::Lossless, comb_encode, comb_decode, empty, all_options[6]},
};

// Available datasets
struct {
  char name[32];
  const char *path;
  int reader;
  double error;
  int compressor_list[20];
} datasets[] = {
    {"us-stocks float", "./data/us-stocks.csv", 0, 1E-3, {0, 1, 2, 3, 4, EOL}},
    {"bird-migration float", "./data/bird-migration.csv", 0, 1E-3, {0, 1, 2, 3, 4, EOL}},
    {"us-stocks timestamp", "./data/us-stocks.csv", 1, 1E-3, {0, 1, 2, 3, 4, 5, 6, EOL}},
    {"bird-migration timestamp", "./data/bird-migration.csv", 1, 1E-3, {0, 1, 2, 3, 4, 5, 6, EOL}},
    {"pcap", "./data/tcpdump.pcap", 2, 1E-3, {0, 1, 5, EOL}},
};

// List of datasets to be evaluated
int dataset_list[] = {0, 1, 2, 3, 4, EOL};
// List of slice lengths to be evaluated
int bsize_list[] = {2000, EOL};

///////////////////////// Setting End ////////////////////////////

int check(double *d0, double *d1, size_t len, double error) {
  if (error == 0) {
    for (int i = 0; i < len; i++) {
      //if (d0[i] != d1[i]) {
      if (memcmp(&(d0[i]), &(d1[i]), 8) != 0) {
        printf("%d: %.16lf(%lx) vs %.16lf(%lx)\n", i, d0[i],
               ((uint64_t *)d0)[i], d1[i], ((uint64_t *)d1)[i]);
        return -1;
      }
    }
  } else {
    for (int i = 0; i < len; i++) {
      if (std::abs(d0[i] - d1[i]) > error) {
        printf("%d(%lf): %lf vs %lf\n", i, d0[i] - d1[i], d0[i], d1[i]);
        return -1;
      }
    }
  }
  return 0;
}

int test_file(FILE *file, int r, int c, int chunk_size, double error) {
  double *d0 = (double *)malloc(chunk_size * sizeof(double));
  uint8_t *d1;
  double *d2 = (double *)malloc(chunk_size * sizeof(double));
  int64_t start;

  // compress
  FILE *fc = fopen("/tmp/tmp.cmp", "w");
  int block = 0;
  while (!feof(file)) {
    int len0 = readers[r](file, d0, chunk_size);
    if (len0 == 0)
      break;

    start = clock();
    int len1 = compressors[c].compress(d0, len0, &d1, error, compressors[c].options);
    compressors[c].perf.cmp_time += clock() - start;
    compressors[c].perf.cmp_size += len1;

    fwrite(&len1, sizeof(len1), 1, fc);
    fwrite(d1, 1, len1, fc);
    free(d1);
    block++;
  }
  fclose(fc);

  // decompress
  rewind(file);
  fc = fopen("/tmp/tmp.cmp", "r");
  block = 0;
  while (!feof(file)) {
    int len0 = readers[r](file, d0, chunk_size);
    if (len0 == 0)
      break;

    int len1;
    (void)!fread(&len1, sizeof(len1), 1, fc);
    d1 = (uint8_t *)malloc(len1);
    (void)!fread(d1, 1, len1, fc);
    start = clock();
    int len2 = compressors[c].decompress(d1, len1, d2, error, compressors[c].options);
    compressors[c].perf.dec_time += clock() - start;
    compressors[c].perf.ori_size += len2 * sizeof(double);

    double terror;
    switch (compressors[c].type) {
    case Lossy:
      terror = error;
      break;
    case Lossless:
      terror = 0;
      break;
    }

    if (len0 != len2 || check(d0, d2, len0, terror)) {
      printf("Check failed, dumping data to tmp.data\n");
      FILE *dump = fopen("tmp.data", "w");
      fwrite(d0, sizeof(double), len0, dump);
      fclose(dump);

      free(d1);
      free(d2);
      free(d0);
      fclose(file);
      return -1;
    }
    free(d1);
    block++;
  }
  fclose(fc);

  free(d0);
  free(d2);
  return 0;
}

void draw_progress(int now, int total, int len) {
  int count = now * len / total;
  int i;
  fprintf(stderr, "Progress: ");
  for (i = 0; i < count; i++) {
    fputc('#', stderr);
  }
  for (; i < len; i++) {
    fputc('-', stderr);
  }
  fputc('\r', stderr);
}

int test_dataset(int ds, int chunk_size) {
  printf("**************************************");
  printf("      Testing on %s(%.8lf)\t", datasets[ds].name, datasets[ds].error);
  printf("**************************************\n");
  fflush(stdout);

  const char *file_path = datasets[ds].path;

  //draw_progress(cur_file, file_cnt, 80);
  FILE *file = fopen(file_path, "rb");
  for (int i = 0; datasets[ds].compressor_list[i] != EOL; i++) {
    if (datasets[ds].compressor_list[i] == SKIP) {
      continue;
    }
    fseek(file, 0, SEEK_SET);
    if (test_file(file, datasets[ds].reader, datasets[ds].compressor_list[i], chunk_size, datasets[ds].error)) {
      printf("Error Occurred while testing %s, skipping\n",
             compressors[datasets[ds].compressor_list[i]].name);
      datasets[ds].compressor_list[i] = SKIP;
    }
  }
  fclose(file);
  //draw_progress(cur_file, file_cnt, 80);
  return 0;
}

void report(int c) {
  std::cout << compressors[c].name << std::setw(20 - strlen(compressors[c].name)) << "\t";
  printf("Compression ratio: %lf\t",
         (double)compressors[c].perf.ori_size / compressors[c].perf.cmp_size);
  printf("Compression speed: %lf MB/s\t",
         (double)compressors[c].perf.ori_size / 1024 / 1024 /
             ((double)compressors[c].perf.cmp_time / CLOCKS_PER_SEC));
  printf("Decompression speed: %lf MB/s\t",
         (double)compressors[c].perf.ori_size / 1024 / 1024 /
             ((double)compressors[c].perf.dec_time / CLOCKS_PER_SEC));
  printf("\n");
  fflush(stdout);
}

int main() {
  double data[1000];
  for (int i = 0; i < 1000; i++) {
    data[i] = (double)rand() / RAND_MAX;
  }

  for (int i = 0; bsize_list[i] != EOL; i++) {
    printf("Current slice length: %d\n", bsize_list[i]);
    fflush(stdout);
    for (int j = 0; dataset_list[j] != EOL; j++) {
      test_dataset(dataset_list[j], bsize_list[i]);
      for (int k = 0; datasets[dataset_list[j]].compressor_list[k] != EOL; k++) {
        if (datasets[dataset_list[j]].compressor_list[k] != SKIP) {
          report(datasets[dataset_list[j]].compressor_list[k]);
        }
        __builtin_memset(&compressors[datasets[dataset_list[j]].compressor_list[k]].perf, 0,
                         sizeof(Perf));
      }
    }
  }
  printf("Test finished\n");
  return 0;
}
