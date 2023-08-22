#pragma comment(lib, "lib/FTD2XX.lib")
#include "inc/circular_buffer.h"
#include "inc/ftd2xx.h"
#include <stdint.h>
#include <stdio.h>

#define MLEN 65536
#define CBUFSZ 262144

typedef struct
{
  uint8_t MSG[MLEN];
  uint32_t SZE;
  uint32_t CNT;
} PKT;

void readConfig(FT_HANDLE ftH)
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
  }

  fprintf(stderr, "Desc:%s SN:%s \n", ftData.Description, ftData.SerialNumber);
}

void writeMillisecond(FILE *fp, cbuf_handle_t cbH)
{
  uint8_t ch;
  uint32_t sze = circular_buf_size(cbH);
  uint32_t idx;

  for (idx=0; idx<sze; idx++)
  {
    circular_buf_get(cbH, &ch);
    fputc(ch, fp);
  }
  if (circular_buf_size(cbH) != 0) {
    fprintf(stderr, "Error, Circ Buf Not Empty\n");
    exit(1);
  }
}

int main(int argc, char *argv[])
{
  FILE *RAW;
  PKT rx;

  FT_HANDLE ftH;
  FT_STATUS ftS;

  unsigned long i = 0, totalBytes = 0, targetBytes = 0;
  unsigned char sampleValue;
  char valueToWrite;

  uint32_t idx = 0;
  uint8_t blankLine[120];
  uint8_t ch;

  uint8_t *CBuff = malloc(CBUFSZ * sizeof(uint8_t));
  int16_t cbstatus;
  cbuf_handle_t cb = circular_buf_init(CBuff, CBUFSZ);
  bool full = circular_buf_full(cb);
  bool empty = circular_buf_empty(cb);

  for (idx = 0; idx < 120; idx++)
  {
    blankLine[idx] = '\b';
  }
  blankLine[119] = '\0';

  memset(rx.MSG, 0, 65536);

  /* Insert Argument Parser Here */
  if (argc == 2)
  {
    targetBytes = 8184 * atoi(argv[1]); // number of milliseconds to record
    // targetBytes = 16368 * atoi(argv[1]);
  }
  else
  {
    targetBytes = 65472; // 65472 is 8 ms of data
    // targetBytes = 130944; // 65472 is 8 ms of data
  }

  /* After Arguments Parsed, Open [Optional] Files */
  RAW = fopen("L1IFDATA.raw", "wb");
  fprintf(stderr, "Looking for %d Bytes (N*8184)\n", targetBytes);
  // ftS = FT_OpenEx("A", FT_OPEN_BY_SERIAL_NUMBER, &fthandle1);
  // ftS = FT_OpenEx("GPS4WEIT9", FT_OPEN_BY_SERIAL_NUMBER, &fthandle1);
  // ftS = FT_OpenEx("USB<->GPS", FT_OPEN_BY_DESCRIPTION, &fthandle1);
  ftS = FT_Open(0, &ftH);
  if (ftS != FT_OK)
  {
    fprintf(stderr, "open device status not ok %d\n", ftS);
    exit(1);
  }

  readConfig(ftH);
  /* */
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
  // FT_Close(fthandle1); // Don't close it, just purge it.

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
          circular_buf_try_put(cb, rx.MSG[i]);
          totalBytes += 1;
          if (totalBytes % 8184 == 0)
          {
//            fprintf(stderr, "CB Size: %zu\n", circular_buf_size(cb));
            writeMillisecond(RAW, cb);
            if (totalBytes == targetBytes)
              break;
          }
        }
      }                          // end buffer read not too big
      memset(rx.MSG, 0, rx.CNT); // May not be necessary
    }                            // end Read was not an error
  }                              // end while loop

  free(CBuff);

  if (FT_W32_PurgeComm(ftH, PURGE_TXCLEAR | PURGE_RXCLEAR))
    printf("\nPurging Buffers\n");
  ftS = FT_Close(ftH);
  fclose(RAW);
}