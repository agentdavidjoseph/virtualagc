/*
 * Copyright 2016 Ronald S. Burkey <info@sandroid.org>
 *
 * This file is part of yaAGC.
 *
 * yaAGC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * yaAGC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with yaAGC; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * In addition, as a special exception, Ronald S. Burkey gives permission to
 * link the code of this program with the Orbiter SDK library (or with
 * modified versions of the Orbiter SDK library that use the same license as
 * the Orbiter SDK library), and distribute linked combinations including
 * the two. You must obey the GNU General Public License in all respects for
 * all of the code used other than the Orbiter SDK library. If you modify
 * this file, you may extend this exception to your version of the file,
 * but you are not obligated to do so. If you do not wish to do so, delete
 * this exception statement from your version.
 *
 * Filename:    processConsoleDebuggingCommand.c
 * Purpose:     Process a user command (virtual AGC is idle).
 * Compiler:    GNU gcc.
 * Contact:     Ron Burkey <info@sandroid.org>
 * Reference:   http://www.ibiblio.org/apollo/index.html
 * Mods:        2016-09-03 RSB  Wrote.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "yaAGCb1.h"

char *
printFlatAddress(uint16_t flatAddress)
{
  static char string[32];
  if (flatAddress < 06000)
    sprintf(string, "%04o", flatAddress);
  else if (flatAddress < MEMORY_SIZE)
    sprintf(string, "%02o,%04o", flatAddress / 02000,
        (flatAddress % 02000) + 06000);
  else
    sprintf(string, "? (%o)", flatAddress);
  return (string);
}

void
processConsoleDebuggingCommand(char *command)
{
  unsigned u, flatAddress, bank, offset;
  int i, index;
  char c, *s;

  if (command != NULL)
    {
      s = strstr(command, "\n");
      if (s != NULL)
        *s = 0;
      if (!strcasecmp(command, "c"))
        {
          agc.instructionCountDown = -1;
          return;
        }
      if (!strcasecmp(command, "s") || !strcasecmp(command, "t"))
        {
          agc.instructionCountDown = 1;
          return;
        }
      if (2 == sscanf(command, "%c%d", &c, &i)
          && (c == 's' || c == 'S' || c == 't' || c == 'T'))
        {
          if (i >= 1)
            {
              printf("Stepping %d instructions.\n", i);
              agc.instructionCountDown = i + 1;
            }
          else if (i < 0)
            {
              printf("Free running.\n");
              agc.instructionCountDown = i;
            }
          else
            agc.instructionCountDown = 0;
          return;
        }
      if (3 == sscanf(command, "%c%o,%o", &c, &bank, &offset))
        {
          if (bank < 040 && offset >= 06000 && offset <= 07777)
            {
              flatAddress = bank * 02000 + (offset & 01777);
              if (c == 'e' || c == 'E')
                {
                  goto examineMemory;
                }
              if (c == 'b' || c == 'B')
                {
                  int i;
                  breakpoint: ;
                  for (i = 0; i < numBreaksOrWatches; i++)
                    if (breaksOrWatches[i] == flatAddress)
                      {
                        printf("Removing break/watch %s\n",
                            printFlatAddress(flatAddress));
                        if (i < numBreaksOrWatches - 1)
                          memmove(&breaksOrWatches[i], &breaksOrWatches[i + 1],
                              2);
                        numBreaksOrWatches--;
                        goto reprompt;
                      }
                  if (numBreaksOrWatches < MAX_BREAKS_OR_WATCHES)
                    {
                      breaksOrWatches[numBreaksOrWatches++] = flatAddress;
                      printf("Adding break/watch %s\n",
                          printFlatAddress(flatAddress));
                    }
                  else
                    {
                      printf(
                          "Too many breaks or watches (max %d) to add another\n",
                          MAX_BREAKS_OR_WATCHES);
                    }
                  goto reprompt;
                }
            }
        }
      else if (2 == sscanf(command, "%c%o", &c, &flatAddress))
        {
          if (flatAddress < sizeof(agc.memory) / 2)
            {
              if (c == 'e' || c == 'E')
                {
                  int i;
                  examineMemory: ;
                  for (i = 0; i < 24 && flatAddress < sizeof(agc.memory) / 2;
                      i++, flatAddress++)
                    {
                      if (flatAddress < 06000)
                        printf("%04o:  %06o\n", flatAddress,
                            agc.memory[flatAddress]);
                      else
                        printf("%02o,%04o:  %06o\n", flatAddress / 02000,
                            06000 + (flatAddress % 02000),
                            agc.memory[flatAddress]);
                    }
                  goto reprompt;
                }
              if (c == 'b' || c == 'B')
                goto breakpoint;
            }
        }
      else if (!strcasecmp(command, "b"))
        {
          int i;
          if (numBreaksOrWatches == 0)
            printf("No breaks or watches to remove.\n");
          for (i = 0; i < numBreaksOrWatches; i++)
            printf("Removing break/watch %s\n",
                printFlatAddress(breaksOrWatches[i]));
          numBreaksOrWatches = 0;
          goto reprompt;
        }
      else if (!strcasecmp(command, "b?"))
        {
          int i;
          if (numBreaksOrWatches == 0)
            printf("No breaks or watches.\n");
          for (i = 0; i < numBreaksOrWatches; i++)
            printf("Break/watch %d:  %s\n", i,
                printFlatAddress(breaksOrWatches[i]));
          goto reprompt;
        }
      if (!strcasecmp(command, "q"))
        {
          exit(0);
        }
      if (*command)
        {
          printf("         c  ---  continuous run\n");
          printf("    s or t  ---  single step\n");
          printf("  sN or tN  ---  N single steps.  If N<0, free running\n");
          printf("eN or eB,O  ---  examine memory at memory location\n");
          printf("bN or bB,0  ---  add/delete breakpoint at location\n");
          printf("         b  ---  remove all breakpoints\n");
          printf("        b?  ---  list all breakpoints\n");
          printf("         q  ---  quit\n");
        }
    }
  for (u = 0; u < MAX_LINE_LENGTH; u++)
    printf("-");
  printf("\n");
  printf("MCT: %ld   Elapsed: %ld ms   Paused: %ld ms   CPU: %ld ms\n",
      agc.countMCT, (getTimeNanoseconds() - agc.startTimeNanoseconds) / 1000000,
      agc.pausedNanoseconds / 1000000,
      (agc.countMCT * 12)/1024);
  printf(
      "  A: %06o   IN0: %05o   OUT0: %05o     Bank: %03o   CYR: %05o   ZRUPT: %05o   TIME1: %06o\n",
      regA, regIN0, regOUT0, regBank >> 10, regCYR, regZRUPT, ctrTIME1);
  printf(
      "  Q: %06o   IN1: %05o   OUT1: %05o                  SR: %05o   BRUPT: %05o   TIME2: %06o\n",
      regQ, regIN1, regOUT1, regSR, regBRUPT, ctrTIME2);
  printf(
      "  Z: %06o   IN2: %05o   OUT2: %05o   Inhint: %-3d   CYL: %05o   ARUPT: %05o   TIME3: %06o\n",
      regZ, regIN2, regOUT2, regInhint, regCYL, regARUPT, ctrTIME3);
  printf(
      " LP: %06o   IN3: %05o   OUT3: %05o                  SL: %05o   QRUPT: %05o   TIME4: %06o\n",
      regLP, regIN3, regOUT3, regSL, regQRUPT, ctrTIME4);
  printf(
      "                           OUT4: %05o                              INDEX: %05o\n",
      regOUT4, agc.INDEX);

  if (regZ< 06000)
  flatAddress = regZ;
  else
  flatAddress = flatten(regBank, regZ);
  index = listingAddresses[flatAddress];
  if (index >= 0)
    {
      int i, start, end, context = 5;

      start = index - context;
      if (start < 0)
        start = 0;
      end = index + context;
      if (end >= MEMORY_SIZE)
        end = MEMORY_SIZE - 1;
      for (i = start; i <= end; i++)
        {
          printf("%c%s\n", (i == index) ? '>' : ' ', bufferedListing[i]);
        }
    }
  reprompt: ;
  printf("> "); // Re-prompt.
  fflush(stdout);
}
