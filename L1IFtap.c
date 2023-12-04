#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#pragma comment(lib, "lib/FTD2XX.lib")
#ifdef WINUDP
#pragma comment(lib, "Ws2_32.lib")
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include "inc/ftd2xx.h"
#include "inc/version.h"
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
#define BYTESPERMS 8184
#define MSPERSEC 1000
// #define DEBUG

typedef struct
{
  uint8_t MSG[MLEN];
  uint32_t SZE;
  uint32_t CNT;
} PKT;

typedef struct
{
  int32_t lComPortNumber;
  uint32_t ftDriverVer;
  FT_PROGRAM_DATA ftData;
  FT_HANDLE ftH;
} FT_CFG;

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
  FT_CFG ftC;
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
#ifdef DEBUG
          printf("%s\n", cfg->baseFname);
#endif
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

void readFTDIConfig(FT_CFG *cfg)
{
  FT_STATUS ftS;
  ftS = FT_GetDriverVersion(cfg->ftH, &cfg->ftDriverVer);
  if (ftS != FT_OK)
  {
    fprintf(stderr, "Couldn't read FTDI driver version.\n");
  }

  ftS = FT_SetTimeouts(cfg->ftH, 500, 500);
  if (ftS != FT_OK)
  {
    fprintf(stderr, "timeout A status not ok %d\n", ftS);
    exit(1);
  }

  ftS = FT_EE_Read(cfg->ftH, &cfg->ftData);
  if (ftS != FT_OK)
  {
    fprintf(stderr, "FTDI EE Read did not succeed! %d\n", ftS);
  }

  ftS = FT_GetComPortNumber(cfg->ftH, &cfg->lComPortNumber);
  if (ftS != FT_OK)
  {
    fprintf(stderr, "FTDI Get Com Port Failed! %d\n", ftS);
  }
  ftS = FT_SetLatencyTimer(cfg->ftH, 2);
  ftS = FT_SetUSBParameters(cfg->ftH, 0x10000, 0x10000);
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

void writeToBinFile(CONFIG *cfg, PKT *p)
{
  int32_t idx = 0;
  uint8_t byteData = 0, upperNibble, lowerNibble;
  int8_t valueToWrite = 0;
  //  printf("Convert file size: %d\n", fSize);

  for (idx = 0; idx < p->CNT; idx++)
  {
    byteData = p->MSG[idx];
    upperNibble = (byteData & 0x30) >> 4;
    lowerNibble = (byteData & 0x03);
    switch (cfg->FNHN == true ? upperNibble : lowerNibble)
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
    fputc(valueToWrite, cfg->ofp);
    switch (cfg->FNHN == true ? lowerNibble : upperNibble)
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
    fputc(valueToWrite, cfg->ofp);
  }
}

void convertFile(CONFIG *cfg)
{
  cfg->ifp = fopen(cfg->sampFname, "rb");
  if (cfg->ifp == NULL)
  {
    fprintf(stderr, "No such file %s\n", cfg->sampFname);
    exit(1);
  }
  else
  {
    cfg->ofp = fopen(cfg->outFname, "wb");
    if (cfg->ofp == NULL)
    {
      fprintf(stderr, "Can't open output file\n");
      exit(1);
    }
    else
    {
      raw2bin(cfg->ofp, cfg->ifp, cfg->FNHN);
    }
  }
  fclose(cfg->ifp);
  fclose(cfg->ofp);
  exit(0);
}

int main(int argc, char *argv[])
{
#ifdef WINUDP
  WSADATA wsaData;
  int32_t iResult; // WSA
#endif
  CONFIG cnfg;
  char ManufacturerBuf[32];
  char ManufacturerIdBuf[16];
  char DescriptionBuf[64];
  char SerialNumberBuf[16];

  cnfg.ftC.ftData.Signature1 = 0x00000000;
  cnfg.ftC.ftData.Signature2 = 0xffffffff;
  cnfg.ftC.ftData.Version = 0x00000003; // 2232H extensions
  cnfg.ftC.ftData.Manufacturer = ManufacturerBuf;
  cnfg.ftC.ftData.ManufacturerId = ManufacturerIdBuf;
  cnfg.ftC.ftData.Description = DescriptionBuf;
  cnfg.ftC.ftData.SerialNumber = SerialNumberBuf;

  PKT rx;
  FT_STATUS ftS;

  float sampleTime = 0.0;
  unsigned long i = 0, totalBytes = 0, targetBytes = 0;
  unsigned char sampleValue;
  char valueToWrite;

  uint32_t idx = 0;
  int32_t len = 0;
  uint8_t blankLine[120];
  uint8_t ch;

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

  if (cnfg.convertFile == true)
  {
    fprintf(stdout, "%s -> %s\n", cnfg.sampFname, cnfg.outFname);
    convertFile(&cnfg);
  }
  else
  {
    cnfg.ofp = fopen(cnfg.outFname, "wb");
  }
#ifdef DEBUG
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

  targetBytes = BYTESPERMS * cnfg.sampMS;
  sampleTime = (float)(targetBytes / BYTESPERMS) / MSPERSEC;

  //  fprintf(stdout, "%d ms requested.\n", cnfg.sampMS);
  fprintf(stdout, "Collecting %10lu Bytes (Nms*8184) [%6.3f sec] in %s\n",
          targetBytes, sampleTime, cnfg.outFname);

  // ftS = FT_Open(0, &ftH);
  ftS = FT_OpenEx("USB<->GPS A", FT_OPEN_BY_DESCRIPTION, &cnfg.ftC.ftH);
  if (ftS != FT_OK)
  {
    fprintf(stderr, "open device status not ok %d\n", ftS);
    exit(1);
  }

#ifdef WINUDP
  iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (iResult != 0)
  {
    fprintf(stderr, "WSAStartup failed: %d\n", iResult);
    return 1;
  }
#endif

  readFTDIConfig(&cnfg.ftC);
#ifdef DEBUG
  fprintf(stdout, "ftDrvr:0x%x  ", cnfg.ftC.ftDriverVer);
  fprintf(stderr, "FIFO:%s  ",
          (cnfg.ftC.ftData.IFAIsFifo7 != 0) ? "Yes" : "No");
  if (cnfg.ftC.lComPortNumber == -1)
  {
    fprintf(stderr, "No COM port assigned\n");
  }
  else
  {
    fprintf(stderr, "COM Port: %d ", cnfg.ftC.lComPortNumber);
  }

  fprintf(stderr, "%s %s SN:%s \n", cnfg.ftC.ftData.Description, cnfg.ftC.ftData.Manufacturer,
          cnfg.ftC.ftData.SerialNumber);
#endif

  ftS = FT_GetQueueStatus(cnfg.ftC.ftH, &rx.CNT);
#ifdef DEBUG
  fprintf(stderr, "Bytes In Queue: %d   ", rx.CNT);
#endif

  ftS = FT_Purge(cnfg.ftC.ftH, FT_PURGE_RX | FT_PURGE_TX); // Purge both Rx and Tx buffers
  if (ftS != FT_OK)
  {
    fprintf(stderr, "Couldn't purge FTDI FIFO buffer! %d\n", ftS);
    fclose(cnfg.ofp);
    exit(1);
  }

  ftS = FT_GetQueueStatus(cnfg.ftC.ftH, &rx.CNT);
#ifdef DEBUG
  fprintf(stderr, "Bytes In Queue: %d\n", rx.CNT);
#endif

  while (totalBytes < targetBytes)
  {
    ftS = FT_GetQueueStatus(cnfg.ftC.ftH, &rx.CNT);
    fprintf(stdout, "%s", blankLine);
    fprintf(stdout, "Collected: %10lu Bytes [%10lu bytes to go with %5d in queue]",
            totalBytes, targetBytes - totalBytes, rx.CNT);
    rx.SZE = rx.CNT; // tell it you want the whole buffer
    ftS = FT_Read(cnfg.ftC.ftH, rx.MSG, rx.SZE, &rx.CNT);
    if (ftS != FT_OK)
    {
      fprintf(stderr, "Status not OK %d\n", ftS);
      exit(1);
    }
    else
    {
      if ((rx.CNT < 65536) && (rx.CNT > 0))
      {
        if ((totalBytes + rx.CNT) > targetBytes)
        {
          rx.CNT = targetBytes - totalBytes;
          // printf("\nrem:%d %d %d %d\n", totalBytes, targetBytes, rx.CNT, bytesToWrite);
        }
        if (cnfg.logfile == true)
        {
          fwrite(rx.MSG, sizeof(uint8_t), rx.CNT, cnfg.ofp);
        }
        else
        {
          writeToBinFile(&cnfg, &rx);
        }
        totalBytes += rx.CNT;
      }                          // end buffer read not too big
      memset(rx.MSG, 0, rx.CNT); // May not be necessary
    }                            // end Read was not an error
  }                              // end while loop

  if (FT_W32_PurgeComm(cnfg.ftC.ftH, PURGE_TXCLEAR | PURGE_RXCLEAR))
  {
    fprintf(stdout, "\n\t   %10lu Bytes written to %s",
            cnfg.logfile == true ? totalBytes : totalBytes * 2, cnfg.outFname);
  }
  ftS = FT_Close(cnfg.ftC.ftH);
  fclose(cnfg.ofp);
#ifdef WINUDP
  WSACleanup();
#endif
  return 0;
}
