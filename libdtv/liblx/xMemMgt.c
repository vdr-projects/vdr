/*
 *
 * xMemMgt.c: memory management functions of liblx
 *
 *
 * $Revision: 1.1 $
 * $Date: 2001/06/25 12:29:47 $
 * $Author: hakenes $
 *
 *   (C) 1992-2001 Rolf Hakenes <hakenes@hippomi.de>, under the GNU GPL.
 *
 * liblx is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * liblx is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You may have received a copy of the GNU General Public License
 * along with liblx; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "liblx.h"

#ifdef DEBUG
void logPrintf(int, char *, ...);
#endif

static struct MEM_CHUNK *xRememberKey = NULL;

static struct MEM_CHUNK **xRememberPtr = &xRememberKey;

unsigned long xAllocatedMemory = 0;

/*************************************************************************
 *                                                                       *
 *     function  :   xMemAlloc                                           *
 *                                                                       *
 *     parameter :   Size - size of the requested memory area            *
 *                                                                       *
 *                   DataPointer - pointer to data pointer               *
 *                                                                       *
 *     return    :   none                                                *
 *                                                                       *
 *-----------------------------------------------------------------------*
 *                                                                       *
 *   xMemAlloc() is a clustered, remembering memory management routine.  *
 *   It uses its own tables for free and used memory blocks on private   *
 *   memory area. With xMemFree(), you can free this memory likewise     *
 *   the C free() routine, with xMemFreeAll() all memory at once.        *
 *   By changing the current remember key with xSetRemember() you can    *
 *   define a local memory area, which can be freed by only one call of  *
 *   xMemFreeAll() (see xSetRemember() / xGetRemember()).                *
 *                                                                       *
 *************************************************************************/

void xMemAllo (Size, DataPointer)

unsigned long Size;
unsigned char **DataPointer;
{
   struct MEM_CHUNK        *MemChunk, *MemChunkPred;
   struct MEM_ENTRY        *MemEntry, *MemEntryPred;
   long int                 NewSize;
   unsigned short           FoundFlag;
#ifdef DEBUG
   unsigned char           *ptr;
#endif

   while (Size % 4) Size++;

   if (Size > (MEM_CHUNK_SIZE - sizeof(struct MEM_CHUNK) - 
                  sizeof(struct MEM_ENTRY)))
   {
      NewSize = Size + sizeof(struct MEM_CHUNK) + sizeof(struct MEM_ENTRY);

      if (MemChunk = (*xRememberPtr))
      {
         do
         {
            MemChunkPred = MemChunk;
         } while (MemChunk = MemChunk->Succ);
      }
      else MemChunkPred = (struct MEM_CHUNK *) &(*xRememberPtr);

      MemChunk = MemChunkPred->Succ = (struct MEM_CHUNK *) malloc (NewSize);
      xAllocatedMemory += NewSize;

#ifdef DEBUG
      for (ptr = (unsigned char *) MemChunk; ptr < (unsigned char *)
            (MemChunk) + NewSize; ptr++)
             *ptr = (((unsigned long)ptr)&1) ? 0x55 : 0xAA;
#endif

      if (!MemChunk)
      {
#ifdef DEBUG
         logPrintf (0, "Not enough memory...\r\n");
#endif
         exit (1);
      }

      MemChunk->Size = NewSize;
      MemChunk->Pred = MemChunkPred;
      MemChunk->Succ = NULL;
      MemChunk->FirstFreeMemEntry = NULL;
      MemChunk->FirstUsedMemEntry =
         MemEntry = (struct MEM_ENTRY *) ((unsigned char *)MemChunk +
                       sizeof(struct MEM_CHUNK));

      MemEntry->Size = Size;
      MemEntry->Pred = (struct MEM_ENTRY *) &MemChunk->FirstUsedMemEntry;
      MemEntry->Succ = NULL;

      *DataPointer = (unsigned char *) ((unsigned char *)MemEntry +
                                           sizeof(struct MEM_ENTRY));
#ifdef DEBUG_CALLS
      logPrintf (0, "xMemAlloc: %x, %d bytes\r\n", *DataPointer, Size);
#endif
      return;
   }

   MemEntry = NULL;
   FoundFlag = 0;

   if (MemChunk = (*xRememberPtr))
   {
      do
      {
         if (MemEntry = MemChunk->FirstFreeMemEntry)
         do
         {
            if (Size <= MemEntry->Size) FoundFlag = 1;
         } while ((FoundFlag == 0) && (MemEntry = MemEntry->Succ));
         MemChunkPred = MemChunk;
      } while ((FoundFlag == 0) && (MemChunk = MemChunk->Succ));
   }
   else MemChunkPred = (struct MEM_CHUNK *) &(*xRememberPtr);

   if (!MemEntry)
   {
      MemChunk = MemChunkPred->Succ =
         (struct MEM_CHUNK *) malloc (MEM_CHUNK_SIZE);
      xAllocatedMemory += MEM_CHUNK_SIZE;

#ifdef DEBUG
      for (ptr = (unsigned char *) MemChunk; ptr < (unsigned char *)
            (MemChunk) + MEM_CHUNK_SIZE; ptr++)
             *ptr = (((unsigned long)ptr)&1) ? 0x55 : 0xAA;
#endif

      if (!MemChunk)
      {
#ifdef DEBUG
         logPrintf (0, "Not enough memory...\r\n");
#endif
         exit (1);
      }

      MemChunk->Size = MEM_CHUNK_SIZE;
      MemChunk->Pred = MemChunkPred;
      MemChunk->Succ = NULL;
      MemChunk->FirstUsedMemEntry = NULL;
      MemChunk->FirstFreeMemEntry =
         MemEntry = (struct MEM_ENTRY *) 
		               ((unsigned char *)MemChunk + sizeof(struct MEM_CHUNK));

      MemEntry->Size = MEM_CHUNK_SIZE - sizeof(struct MEM_CHUNK) - 
	                                       sizeof(struct MEM_ENTRY);
      MemEntry->Pred = (struct MEM_ENTRY *) &MemChunk->FirstFreeMemEntry;
      MemEntry->Succ = NULL;
   }

   NewSize = MemEntry->Size - sizeof(struct MEM_ENTRY) - Size;

   MemEntry->Size = Size;
   *DataPointer = (unsigned char *) 
                     ((unsigned char *)MemEntry + sizeof(struct MEM_ENTRY));

#ifdef DEBUG
   for (ptr = *DataPointer; ptr < (unsigned char *)
            (*DataPointer) + Size; ptr++)
   {
     if (((unsigned long )ptr)&1)
     {  if (*ptr != 0x55)
           logPrintf (0, "freed memory was used\r\n"); }
     else { if (*ptr != 0xAA)
           logPrintf (0, "freed memory was used\r\n"); }
   }
#endif

   if (MemEntry->Succ) 
      ((struct MEM_ENTRY *)MemEntry->Succ)->Pred = MemEntry->Pred;
   ((struct MEM_ENTRY *)MemEntry->Pred)->Succ = MemEntry->Succ;

   if (MemChunk->FirstUsedMemEntry)
      MemChunk->FirstUsedMemEntry->Pred = MemEntry;
   MemEntry->Succ = MemChunk->FirstUsedMemEntry;
   MemChunk->FirstUsedMemEntry = MemEntry;
   MemEntry->Pred = (struct MEM_ENTRY *) &MemChunk->FirstUsedMemEntry;

   if (NewSize > 0)
   {
      MemEntry = (struct MEM_ENTRY *) 
	          ((unsigned char *)MemEntry + sizeof(struct MEM_ENTRY) + Size);
      MemEntry->Size = NewSize;

      if (MemChunk->FirstFreeMemEntry)
         MemChunk->FirstFreeMemEntry->Pred = MemEntry;
      MemEntry->Succ = MemChunk->FirstFreeMemEntry;
      MemChunk->FirstFreeMemEntry = MemEntry;
      MemEntry->Pred = (struct MEM_ENTRY *) &MemChunk->FirstFreeMemEntry;
   }
#ifdef DEBUG_CALLS
   logPrintf (0, "xMemAlloc: %x, %d bytes\r\n", *DataPointer, Size);
#endif
   return;
}



/*************************************************************************
 *                                                                       *
 *     function  :   xMemFree                                            *
 *                                                                       *
 *     parameter :   DataPointer - data pointer                          *
 *                                                                       *
 *     return    :   none                                                *
 *                                                                       *
 *-----------------------------------------------------------------------*
 *                                                                       *
 *   xMemFree() frees with xMemAlloc() allocated memory.                 *
 *                                                                       *
 *************************************************************************/

void xMemFre (DataPointer)

unsigned char *DataPointer;
{
   struct MEM_CHUNK            *MemChunk, *MemChunkPred;
   struct MEM_ENTRY            *MemEntry, *TempEntry, *PredEntry, *SuccEntry;
   unsigned short               FoundFlag;
#ifdef DEBUG
   unsigned char           *ptr;
#endif

   if (!DataPointer)
   {
      return;
   }
   else
   {
      MemEntry = NULL;
      FoundFlag = 0;

      if (MemChunk = (*xRememberPtr))
      do
      {
         if (MemEntry = MemChunk->FirstUsedMemEntry)
         do
         {
            if (DataPointer == (unsigned char *) ((unsigned char *) MemEntry +
                   sizeof(struct MEM_ENTRY))) FoundFlag = 1;
         } while ((FoundFlag == 0) && (MemEntry = MemEntry->Succ));
      } while ((FoundFlag == 0) && (MemChunk = MemChunk->Succ));

      if (FoundFlag == 1)
      {
#ifdef DEBUG_CALLS
   logPrintf (0, "xMemFree: %x, %d bytes\r\n", DataPointer, MemEntry->Size);
#endif
         if (MemEntry->Succ)
            ((struct MEM_ENTRY *)MemEntry->Succ)->Pred = MemEntry->Pred;
         ((struct MEM_ENTRY *)MemEntry->Pred)->Succ = MemEntry->Succ;

         if (!MemChunk->FirstUsedMemEntry)
         {
            if (MemChunk->Succ)
               ((struct MEM_CHUNK *)MemChunk->Succ)->Pred = MemChunk->Pred;
            ((struct MEM_CHUNK *)MemChunk->Pred)->Succ = MemChunk->Succ;
            if (xAllocatedMemory > 0) xAllocatedMemory -= MemChunk->Size;
            free (MemChunk);
            return;
         }

         FoundFlag = 0;
         PredEntry = NULL;
         SuccEntry = NULL;
         if (TempEntry = MemChunk->FirstFreeMemEntry)
         do
         {
            if ((struct MEM_ENTRY *)((unsigned char *)TempEntry +
                  TempEntry->Size + sizeof(struct MEM_ENTRY)) == MemEntry)
            {
               FoundFlag ++;
               PredEntry = TempEntry;
            }
            if ((struct MEM_ENTRY *)((unsigned char *)MemEntry +
                  MemEntry->Size + sizeof(struct MEM_ENTRY)) == TempEntry)
            {
               FoundFlag ++;
               SuccEntry = TempEntry;
            }
         } while ((FoundFlag != 2) && (TempEntry = TempEntry->Succ));

         if (PredEntry)
         {
            if (SuccEntry)
            {
               /* Vorgdnger + Nachfolger */

               if (SuccEntry->Succ)
                  ((struct MEM_ENTRY *)SuccEntry->Succ)->Pred = SuccEntry->Pred;
               ((struct MEM_ENTRY *)SuccEntry->Pred)->Succ = SuccEntry->Succ;

               PredEntry->Size += MemEntry->Size + sizeof(struct MEM_ENTRY) +
                                  SuccEntry->Size + sizeof(struct MEM_ENTRY);
            }
            else
            {
               /* nur Vorgaenger */

               PredEntry->Size += MemEntry->Size + sizeof(struct MEM_ENTRY);
            }
#ifdef DEBUG
            for (ptr = (unsigned char *) (PredEntry) + sizeof(struct MEM_ENTRY);
                ptr < (unsigned char *) (PredEntry) + sizeof(struct MEM_ENTRY) +
                PredEntry->Size; ptr++)
                  *ptr = (((unsigned long)ptr)&1) ? 0x55 : 0xAA;
#endif
         }
         else
         {
            if (SuccEntry)
            {
               /* nur Nachfolger */

               if (SuccEntry->Succ)
                  ((struct MEM_ENTRY *)SuccEntry->Succ)->Pred = SuccEntry->Pred;
               ((struct MEM_ENTRY *)SuccEntry->Pred)->Succ = SuccEntry->Succ;

               MemEntry->Size += SuccEntry->Size + sizeof(struct MEM_ENTRY);
            }

            if (MemChunk->FirstFreeMemEntry)
               MemChunk->FirstFreeMemEntry->Pred = MemEntry;
            MemEntry->Succ = MemChunk->FirstFreeMemEntry;
            MemChunk->FirstFreeMemEntry = MemEntry;
            MemEntry->Pred = (struct MEM_ENTRY *) &MemChunk->FirstFreeMemEntry;
#ifdef DEBUG
            for (ptr = (unsigned char *) (MemEntry) + sizeof(struct MEM_ENTRY);
                ptr < (unsigned char *) (MemEntry) + sizeof(struct MEM_ENTRY) +
                MemEntry->Size; ptr++)
                 *ptr = (((unsigned long)ptr)&1) ? 0x55 : 0xAA;
#endif
         }
      }
#ifdef DEBUG_CALLS
   else
   logPrintf (0, "xMemFree: tried to free unallocated data %x\r\n", DataPointer);
#endif
   }
   return;
}



/*************************************************************************
 *                                                                       *
 *     function  :   xMemFreeAll                                         *
 *                                                                       *
 *     parameter :   RememberPtr                                         *
 *                                                                       *
 *     return    :   none                                                *
 *                                                                       *
 *-----------------------------------------------------------------------*
 *                                                                       *
 *   xMemFreeAll() frees all with xMemAlloc() allocated memory. If Re-   *
 *   memberPtr is not NULL, the MEM_CHUNK structure from the specified   *
 *   Address is freed, otherwise the natural MEM_CHUNK will be done.     *
 *                                                                       *
 *************************************************************************/


void xMemFreeAll (RememberPtr)

struct MEM_CHUNK **RememberPtr;
{
   struct MEM_CHUNK            *MemChunk, *MemChunkPred;

   if (RememberPtr)
   {
      if (MemChunkPred = (*RememberPtr))
      do
      {
         MemChunk = MemChunkPred->Succ;
         if (xAllocatedMemory > 0) xAllocatedMemory -= MemChunkPred->Size;
         free (MemChunkPred);
      } while (MemChunkPred = MemChunk);
      *RememberPtr = NULL;
   }
   else
   {
      if (MemChunkPred = (*xRememberPtr))
      do
      {
         MemChunk = MemChunkPred->Succ;
         if (xAllocatedMemory > 0) xAllocatedMemory -= MemChunkPred->Size;
         free (MemChunkPred);
      } while (MemChunkPred = MemChunk);
      *xRememberPtr = NULL;
   }
}


/*************************************************************************
 *                                                                       *
 *     function  :   xMemMerge                                           *
 *                                                                       *
 *     parameter :   RememberPtr                                         *
 *                                                                       *
 *     return    :   none                                                *
 *                                                                       *
 *-----------------------------------------------------------------------*
 *                                                                       *
 *   xMemMerge() merges the memory area pointed to by RememberKey with   *
 *   the currently used in xRememberPtr.                                 *
 *                                                                       *
 *************************************************************************/

void xMemMerge (RememberPtr)

struct MEM_CHUNK **RememberPtr;
{
   struct MEM_CHUNK            *MemChunk, *MemChunkPred;

   if (RememberPtr)
   {
      if (MemChunk = (*xRememberPtr))
      {
         while (MemChunk->Succ) MemChunk = MemChunk->Succ;
         MemChunk->Succ = (*RememberPtr);
         *RememberPtr = NULL;
      }
      else (*xRememberPtr = *RememberPtr);
   }
   return;
}

/*************************************************************************
 *                                                                       *
 *     function  :   xGetRemember                                        *
 *                                                                       *
 *     parameter :   none                                                *
 *                                                                       *
 *     return    :   pointer to a MEM_CHUNK tree                         *
 *                                                                       *
 *-----------------------------------------------------------------------*
 *                                                                       *
 *   xGetRemember() returns the currently used MEM_CHUNK tree.           *
 *                                                                       *
 *************************************************************************/


struct MEM_CHUNK **xGetRemember ()
{
   return (xRememberPtr);
}


/*************************************************************************
 *                                                                       *
 *     function  :   xSetRemember                                        *
 *                                                                       *
 *     parameter :   pointer to a MEM_CHUNK tree                         *
 *                                                                       *
 *     return    :   none                                                *
 *                                                                       *
 *-----------------------------------------------------------------------*
 *                                                                       *
 *   xSetRemember() redefines the currently used MEM_CHUNK pointer. If   *
 *   RememberPtr is NULL, the natural MEM_CHUNK is reloaded.             *
 *                                                                       *
 *************************************************************************/


void xSetRemember (RememberPtr)

struct MEM_CHUNK **RememberPtr;
{
   if (RememberPtr)
      xRememberPtr = RememberPtr;
   else
      xRememberPtr = &xRememberKey;
}

/*************************************************************************
 *                                                                       *
 *     function  :   xPrintMemList                                       *
 *                                                                       *
 *     parameter :   pointer to a MEM_CHUNK tree                         *
 *                                                                       *
 *     return    :   none                                                *
 *                                                                       *
 *-----------------------------------------------------------------------*
 *                                                                       *
 *   xPrintMemList() prints the currently allocated memory blocks of     *
 *   the specified RememberPtr.                                          *
 *                                                                       *
 *************************************************************************/


void xPrintMemList (Remember)

struct MEM_CHUNK **Remember;
{
   struct MEM_CHUNK           *MemChunk;
   struct MEM_ENTRY           *MemEntry;

   fprintf (stderr, "MemChunkPtr = %x\n", (int) Remember);

   if (MemChunk = *Remember)
   do
   {
      fprintf (stderr, "\tMemChunk at %x with Size %d\n", (int) MemChunk, 
                 (int) MemChunk->Size);

      if (MemEntry = MemChunk->FirstFreeMemEntry)
      do
      {
         fprintf (stderr, "\t\tFree MemEntry at %x (%x) with Size %d\n", 
                   (int) MemEntry, (int)((unsigned char *)MemEntry + 
                      sizeof(struct MEM_ENTRY)), (int) MemEntry->Size);

      } while (MemEntry = MemEntry->Succ);

      if (MemEntry = MemChunk->FirstUsedMemEntry)
      do
      {
         fprintf (stderr, "\t\tUsed MemEntry at %x (%x) with Size %d\n", 
                   (int) MemEntry, (int)((unsigned char *)MemEntry + 
                      sizeof(struct MEM_ENTRY)), (int) MemEntry->Size);

      } while (MemEntry = MemEntry->Succ);

   } while (MemChunk = MemChunk->Succ);
   else fprintf (stderr, "\tNo current MemChunk\n");
}


/*************************************************************************
 *                                                                       *
 *     function  :   xGetMemSize                                         *
 *                                                                       *
 *     parameter :   pointer to a MEM_CHUNK tree                         *
 *                                                                       *
 *     return    :   none                                                *
 *                                                                       *
 *-----------------------------------------------------------------------*
 *                                                                       *
 *   xGetMemSize() gets the size of the currently allocated memory       *
 *   blocks of the specified (or natural if NULL) RememberPtr            *
 *                                                                       *
 *************************************************************************/


unsigned long xGetMemSize (RememberPtr)

struct MEM_CHUNK **RememberPtr;
{
   struct MEM_CHUNK           *MemChunk;
   struct MEM_ENTRY           *MemEntry;
   unsigned long               Result = 0;

   if (RememberPtr) MemChunk = *RememberPtr;
   else MemChunk = xRememberKey;

   if (MemChunk)
   do { Result += (unsigned long) MemChunk->Size; }
   while (MemChunk = MemChunk->Succ);

   return (Result);
}


/*************************************************************************
 *                                                                       *
 *     function  :   xSetText                                            *
 *                                                                       *
 *     arguments :   xText - pointer to a string                         *
 *                                                                       *
 *     return    :   pointer to an new allocated string                  *
 *                                                                       *
 *-----------------------------------------------------------------------*
 *                                                                       *
 *   xSetText() allocates memory for the string pointed to by 'xText'    *
 *   and duplicates it.                                                  *
 *                                                                       *
 *************************************************************************/

char *xSetText (xText)

char *xText;
{
   char   *NewText;

   if (!xText) return (NULL);

   xMemAlloc (strlen(xText) + 1, &NewText);
   strcpy (NewText, xText);

   return (NewText);   
}
