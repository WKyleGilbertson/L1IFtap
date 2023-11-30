#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// #pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "lib/FTD2XX.lib")
#include "inc/ftd2xx.h"
#include "inc/circular_buffer.h"
#include "inc/version.h"
// #include <winsock2.h>
// #include <ws2tcpip.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#if !defined(_WIN32) && (defined(__UNIX__) || (defined(__APPLE__)))
#include <time.h>
#include <sys/time.h>
#include "inc/WinTypes.h"
#endif

#define MLEN 65536
#define CBUFFSZ 32768
#define DEBUG

typedef struct
{
  uint8_t MSG[MLEN];
  uint32_t SZE;
  uint32_t CNT;
} PKT;

typedef struct
{
  FILE *ifp;
  FILE *ofp;
  char baseFname[64];
  char sampFname[64];
  char outFname[64];
  bool convertFile;
  bool logfile;
  bool useTimeStamp;
  bool FNHN;
  uint32_t sampMS;
  SWV V;
} CONFIG;

void fileSize(FILE *fp, int32_t *fs)
{
  fseek(fp, 0L, SEEK_END);
  *fs = ftell(fp);
  rewind(fp);
}

#if !defined(_WIN32)
void getISO8601(char datetime[17])
{
  struct timeval curTime;
  gettimeofday(&curTime, NULL);
  strftime(datetime, 17, "%Y%m%dT%H%M%SZ", gmtime(&curTime.tv_sec));
  //	return (datetime);
}
#endif

#if defined(_WIN32)
char *TimeNow(char *TimeString)
{
  SYSTEMTIME st;
  GetSystemTime(&st);
  sprintf(TimeString, "%.2d:%.2d:%.2d.%.3d",
          st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
  return TimeString;
}

void ISO8601(char *TimeString)
{
  SYSTEMTIME st;
  GetSystemTime(&st);
  sprintf(TimeString, "%.4d%.2d%.2dT%.2d%.2d%.2dZ",
          st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}
#endif

void initconfig(CONFIG *cfg)
{
  strcpy(cfg->baseFname, "L1IFDATA");
  strcpy(cfg->sampFname, "L1IFDATA.raw");
  strcpy(cfg->outFname, "L1IFDATA.bin");
  cfg->logfile = false;
  cfg->useTimeStamp = false;
  cfg->FNHN = true;
  cfg->sampMS = 1;
}

void processArgs(int argc, char *argv[], CONFIG *cfg)
{
  static int len, i, ch = ' ';
  static char *usage =
      "usage: L1IFtap [ms] [options]\n"
      "       ms            how many milliseconds of data to collect\n"
      "       -f <filename> write to a different filename than the default\n"
      "       -l [filename] log raw data rather than binary interpretation\n"
      "       -r <filename> read raw log file and translate to binary\n"
      "       -t            use time tag for file name instead of default\n"
      "       -n            use FNLN instead of FNHN\n"
      "       -v            print version information\n"
      "       -?|h          show this usage infomation message\n"
      "  defaults: 1 ms of data logged in binary format as L1IFDATA.bin";

  if (argc > 1)
  {
    //    printf("%d %c\n", argc, argv[1][1]);
    for (i = 1; i < argc; i++)
    {
      if (argv[i][0] == '-')
      {
        ch = argv[i][1];
        switch (ch)
        {
        case '?':
          printf("%s", usage);
          exit(0);
          break;
        case 'h':
          printf("%s", usage);
          exit(0);
          break;
        case 'n':
          cfg->FNHN = false;
          break;
        case 'f':
          if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
          {
            strcpy(cfg->outFname, argv[++i]);
          }
          else
          {
            printf("%s", usage);
            exit(1);
          }
          break;
        case 'l':
          if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
          {
            strcpy(cfg->outFname, argv[++i]);
          }
          else
          {
            strcpy(cfg->outFname, cfg->sampFname);
          }
          cfg->logfile = true;
          break;
        case 'r':
          if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
          {
            strcpy(cfg->sampFname, argv[++i]);
          }
          else
          {
            printf("%s", usage);
            exit(1);
          }
          cfg->convertFile = true;
          cfg->sampMS = 0;
          len = strlen(cfg->sampFname);
          strcpy(cfg->outFname, cfg->sampFname);
          cfg->outFname[len - 3] = '\0';
          strcat(cfg->outFname, "bin");
          break;
        case 't':
          cfg->useTimeStamp = true;
#if defined(_WIN32)
          ISO8601(cfg->baseFname); // ISO8601 for Windows platform
#endif
#if !defined(_WIN32)
          getISO8601(cfg->baseFname); // getISO8601 for GCC platforms
#endif
          printf("%s\n", cfg->baseFname);
          break;
        case 'v':
          fprintf(stdout, "%s: GitCI:%s %s v%.1d.%.1d.%.1d",
                  cfg->V.Name, cfg->V.GitCI, cfg->V.BuildDate,
                  cfg->V.Major, cfg->V.Minor, cfg->V.Patch);
          exit(0);
          break;
        default:
          printf("%s", usage);
          exit(0);
          break;
        }
      }
      else
      {
        cfg->sampMS = atoi(argv[i]);
        //        printf("ms:%d\n", cfg->sampMS);
      }
    }
  }
  else // argc not greater than 1
  {
    //    printf("Do I need this? Default params\n");
    cfg->sampMS = 1;
  }
  if (cfg->useTimeStamp == true)
  {
    strcpy(cfg->outFname, cfg->baseFname);
    if (cfg->logfile == true)
    {
      strcat(cfg->outFname, ".raw");
    }
    else
    {
      strcat(cfg->outFname, ".bin");
    }
  }
}

void readFTDIConfig(FT_HANDLE ftH)
{
  FT_STATUS ftS;
  FT_PROGRAM_DATA ftData;
  uint32_t ftDriverVer;
  int32_t lComPortNumber;
  char ManufacturerBuf[32];
  char ManufacturerIdBuf[16];
  char DescriptionBuf[64];
  char SerialNumberBuf[16];

  ftData.Signature1 = 0x00000000;
  ftData.Signature2 = 0xffffffff;
  ftData.Version = 0x00000003; // 2232H extensions
  ftData.Manufacturer = ManufacturerBuf;
  ftData.ManufacturerId = ManufacturerIdBuf;
  ftData.Description = DescriptionBuf;
  ftData.SerialNumber = SerialNumberBuf;

  ftS = FT_GetDriverVersion(ftH, &ftDriverVer);
  if (ftS == FT_OK)
    fprintf(stderr, "ftDrvr:0x%x  ", ftDriverVer);

  ftS = FT_SetTimeouts(ftH, 500, 500);
  if (ftS != FT_OK)
  {
    fprintf(stderr, "timeout A status not ok %d\n", ftS);
    exit(1);
  }

  ftS = FT_EE_Read(ftH, &ftData);
  if (ftS == FT_OK)
    fprintf(stderr, "FIFO:%s  ",
            (ftData.IFAIsFifo7 != 0) ? "Yes" : "No");
  else
    fprintf(stderr, "Not Okay? %d\n", ftS);

  /* Don't need COM port number.... ?
  ftS = FT_GetComPortNumber(ftH, &lComPortNumber);
  if (ftS == FT_OK)
  {
    if (lComPortNumber == -1)
    {
      fprintf(stderr, "No COM port assigned\n");
    }
    else
    {
      fprintf(stderr, "COM Port: %d ", lComPortNumber);
    }
  } */

  fprintf(stderr, "%s %s SN:%s \n", ftData.Description, ftData.Manufacturer,
          ftData.SerialNumber);
}

void purgeCBUFFtoFile(FILE *fp, cbuf_handle_t cbH, bool raw, bool FNHN)
{
  uint8_t ch;
  uint8_t upperNibble;
  uint8_t lowerNibble;
  int8_t valueToWrite;
  uint32_t sze = circular_buf_size(cbH);
  uint32_t idx;

  for (idx = 0; idx < sze; idx++)
  {
    circular_buf_get(cbH, &ch);
    if (raw == true)
    {
      fputc(ch, fp);
    }
    else
    {
      upperNibble = (ch & 0x30) >> 4;
      lowerNibble = (ch & 0x03);
      switch (FNHN == true ? upperNibble : lowerNibble)
      {
      case 0x00:
        valueToWrite = 1;
        break;
      case 0x01:
        valueToWrite = 3;
        break;
      case 0x02:
        valueToWrite = -1;
        break;
      case 0x03:
        valueToWrite = -3;
        break;
      }
      fputc(valueToWrite, fp);
      switch (FNHN == true ? lowerNibble : upperNibble)
      {
      case 0x00:
        valueToWrite = 1;
        break;
      case 0x01:
        valueToWrite = 3;
        break;
      case 0x02:
        valueToWrite = -1;
        break;
      case 0x03:
        valueToWrite = -3;
        break;
      }
      fputc(valueToWrite, fp);
    }
  }

  if (circular_buf_size(cbH) != 0)
  {
    fprintf(stderr, "Error, Circ Buf Not Empty\n");
    exit(1);
  }
}

void raw2bin(FILE *dst, FILE *src, bool FNHN)
{
  int32_t fSize = 0, idx = 0;
  uint8_t byteData = 0, upperNibble, lowerNibble;
  int8_t valueToWrite = 0;
  fileSize(src, &fSize);
//  printf("Convert file size: %d\n", fSize);

  for (idx = 0; idx < fSize; idx++)
  {
    byteData = fgetc(src);
    upperNibble = (byteData & 0x30) >> 4;
    lowerNibble = (byteData & 0x03);
    switch (FNHN == true ? upperNibble : lowerNibble)
    {
    case 0x00:
      valueToWrite = 1;
      break;
    case 0x01:
      valueToWrite = 3;
      break;
    case 0x02:
      valueToWrite = -1;
      break;
    case 0x03:
      valueToWrite = -3;
      break;
    }
    fputc(valueToWrite, dst);
    switch (FNHN == true ? lowerNibble : upperNibble)
    {
    case 0x00:
      valueToWrite = 1;
      break;
    case 0x01:
      valueToWrite = 3;
      break;
    case 0x02:
      valueToWrite = -1;
      break;
    case 0x03:
      valueToWrite = -3;
      break;
    }
    fputc(valueToWrite, dst);
  }
  fclose(src);
  fclose(dst);
}

int main(int argc, char *argv[])
{
  FILE *RAW;
  PKT rx;

  FT_HANDLE ftH;
  FT_STATUS ftS;

  CONFIG cnfg;

  //  WSADATA wsaData;
  //  int iResult; // WSA
  float sampleTime = 0.0;
  unsigned long i = 0, totalBytes = 0, targetBytes = 0;
  unsigned char sampleValue;
  char valueToWrite;

  uint32_t idx = 0;
  int32_t len = 0;
  uint8_t blankLine[120];
  uint8_t ch;

  int16_t cbstatus;
  uint8_t CBuff[CBUFFSZ];
  cbuf_handle_t cb = circular_buf_init(CBuff, CBUFFSZ);
  bool full = circular_buf_full(cb);
  bool empty = circular_buf_empty(cb);

  cnfg.V.Major = MAJOR_VERSION;
  cnfg.V.Minor = MINOR_VERSION;
  cnfg.V.Patch = PATCH_VERSION;
#ifdef CURRENT_HASH
  strncpy(cnfg.V.GitCI, CURRENT_HASH, 40);
#endif
#ifdef CURRENT_DATE
  strncpy(cnfg.V.BuildDate, CURRENT_DATE, 16);
#endif
#ifdef CURRENT_NAME
  strncpy(cnfg.V.Name, CURRENT_NAME, 10);
#endif

  for (idx = 0; idx < 120; idx++)
  {
    blankLine[idx] = '\b';
  }
  blankLine[119] = '\0';

  memset(rx.MSG, 0, 65536);

  initconfig(&cnfg);
  processArgs(argc, argv, &cnfg);

#ifdef DEBUG
  if (cnfg.convertFile == true)
  {
    fprintf(stdout, "%s -> %s\n", cnfg.sampFname, cnfg.outFname);
  }
  fprintf(stdout, "base:%s out:%s samp:%s ",
          cnfg.baseFname, cnfg.outFname, cnfg.sampFname);
  fprintf(stdout, "Raw? %s, TS:%s CF: %s FNHN? %s ",
          cnfg.logfile == true ? "yes" : "no",
          cnfg.useTimeStamp == true ? "yes" : "no",
          cnfg.convertFile == true ? "yes" : "no",
          cnfg.FNHN == true ? "yes" : "no");
  fprintf(stdout, "ms: %d\n", cnfg.sampMS);
#endif

  /* After Arguments Parsed, Open [Optional] Files */
  if (cnfg.convertFile == false)
  {
    cnfg.ofp = fopen(cnfg.outFname, "wb");
  }
  else
  {
    cnfg.ifp = fopen(cnfg.sampFname, "rb");
    if (cnfg.ifp == NULL)
    {
      printf("No such file %s\n", cnfg.sampFname);
      exit(1);
    }
    else
    {
      cnfg.ofp = fopen(cnfg.outFname, "wb");
      if (cnfg.ofp == NULL)
      {
        printf("Can't open output file\n");
        exit(1);
      }
      else
      {
        raw2bin(cnfg.ofp, cnfg.ifp, cnfg.FNHN);
      }
    }
    fclose(cnfg.ifp);
    fclose(cnfg.ofp);
    exit(0);
  }

  targetBytes = 8184 * cnfg.sampMS;
  sampleTime = (float)(targetBytes / 8184) / 1000;

  fprintf(stderr, "Looking for %ld Bytes (N*8184) %6.3f sec\n",
          targetBytes, sampleTime);
  ftS = FT_Open(0, &ftH);
  if (ftS != FT_OK)
  {
    fprintf(stderr, "open device status not ok %d\n", ftS);
    exit(1);
  }

  /* If using UDP, set it up
  iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0)
  {
    fprintf(stderr, "WSAStartup failed: %d\n", iResult);
    return 1;
  } */

  readFTDIConfig(ftH);
  ftS = FT_GetQueueStatus(ftH, &rx.CNT);
  fprintf(stderr, "Bytes In Queue: %d   ", rx.CNT);

  ftS = FT_Purge(ftH, FT_PURGE_RX | FT_PURGE_TX); // Purge both Rx and Tx buffers
  if (ftS == FT_OK)
  {
    // FT_Purge OK
  }
  else
  {
    // FT_Purge failed
  }

  ftS = FT_GetQueueStatus(ftH, &rx.CNT);
  fprintf(stderr, "Bytes In Queue: %d\n", rx.CNT);

  while (totalBytes < targetBytes)
  {
    ftS = FT_GetQueueStatus(ftH, &rx.CNT);
    printf("%s", blankLine);
    printf("bytes in RX queue %d ... ", rx.CNT);
    rx.SZE = rx.CNT; // tell it you want the whole buffer
    ftS = FT_Read(ftH, rx.MSG, rx.SZE, &rx.CNT);
    if (ftS != FT_OK)
    {
      fprintf(stderr, "Status not OK %d\n", ftS);
      exit(1);
    }
    else
    {
      if ((rx.CNT < 65536) && (rx.CNT > 0))
      {
        for (i = 0; i < rx.CNT; i++)
        {
          cbstatus = circular_buf_try_put(cb, rx.MSG[i]);
          totalBytes += 1;
          if (totalBytes % 8184 == 0) // 8184 Bytes = 1 ms of data
          {
            // fprintf(stderr, "CB Size: %zu\n", circular_buf_size(cb));
            // Write to UDP stream or copy 1 ms of data, then put it to a file and
            purgeCBUFFtoFile(cnfg.ofp, cb, cnfg.logfile, cnfg.FNHN);
            // purgeCBUFFtoFile(RAW, cb);
            if (totalBytes == targetBytes)
              break;
          }
        }
      }                          // end buffer read not too big
      memset(rx.MSG, 0, rx.CNT); // May not be necessary
    }                            // end Read was not an error
  }                              // end while loop

  if (FT_W32_PurgeComm(ftH, PURGE_TXCLEAR | PURGE_RXCLEAR))
    printf("\nPurging Buffers\n");
  ftS = FT_Close(ftH);
  fclose(cnfg.ofp);
  //  WSACleanup();
  return 0;
}
