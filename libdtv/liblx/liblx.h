/*
 *
 * liblx.h: definitions necessary for the liblx package
 *
 *
 * $Revision: 1.2 $
 * $Date: 2001/06/25 19:39:00 $
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

#ifndef LIBLX_H
#define LIBLX_H

#ifndef NULL
#define NULL 0
#endif


/*
 *
 *   list support structures
 *
 */
struct NODE
{
   struct NODE                 *Succ;
   struct NODE                 *Pred;
   char                        *Name;
   unsigned short               HashKey;
};

struct LIST
{
   struct NODE                 *Head;
   struct NODE                 *Tail;
   char                        *Name;
   unsigned long                Size;
};


/*
 *
 *   memory managment structures
 *
 */
struct MEM_ENTRY
{
   struct MEM_ENTRY        *Succ;
   struct MEM_ENTRY        *Pred;
   unsigned long            Size;
};

struct MEM_CHUNK
{
   struct MEM_CHUNK        *Succ;
   struct MEM_CHUNK        *Pred;
   unsigned long            Size;
   struct MEM_ENTRY        *FirstFreeMemEntry;
   struct MEM_ENTRY        *FirstUsedMemEntry;
};

#ifdef __cplusplus
extern "C" {
#endif

/*
 *
 *   list functions (package xList)
 *
 */
    unsigned short      xHashKey (char *);
    struct LIST        *xNewList (char *);
    struct NODE        *xNewNode (char *, unsigned long);
    struct NODE        *xFindName (struct LIST *, char *);
/*
 *
 *   memory management
 *
 */
    void                xMemAllo (unsigned long, unsigned char **);
    void                xMemFre (unsigned char *);
    void                xMemFreeAll (struct MEM_CHUNK **);
    void                xMemMerge (struct MEM_CHUNK **);
    struct MEM_CHUNK  **xGetRemember (void);
    void                xSetRemember (struct MEM_CHUNK **);
    void                xPrintMemList (struct MEM_CHUNK **);
    unsigned long       xGetMemSize (struct MEM_CHUNK **);
extern unsigned long    xAllocatedMemory;
    char               *xSetText (char *);

#ifdef __cplusplus
}
#endif


#define     MEM_CHUNK_SIZE    65536

#define xMemAlloc(size, ptr) \
   xMemAllo (((unsigned long)((size))), ((unsigned char **)((ptr))))
#define xMemFree(ptr) xMemFre (((unsigned char *)((ptr))))
/*
 *
 *   list support macros
 *
 */
/*---------------------------------------------------------------------*
 |                                                                     |
 |   xCreateNode (NodeStruct,Name) allocates a correctly sized and     |
 |   typed node struct.                                                |
 |                                                                     |
 *---------------------------------------------------------------------*/
#define xCreateNode(NodeStruct,Name) \
   (NodeStruct) = (void *) xNewNode(Name, sizeof(*(NodeStruct)))


/*---------------------------------------------------------------------*
 |                                                                     |
 |      xSize (List) scans for the ->Size field of a list struct       |
 |                                                                     |
 *---------------------------------------------------------------------*/
#define xSize(List) ((List) ? ((struct LIST *)(List))->Size : 0)


/*---------------------------------------------------------------------*
 |                                                                     |
 |    xName (NodeStruct) scans for the ->Node.Name of a node struct    |
 |                                                                     |
 *---------------------------------------------------------------------*/
#define xName(NodeStruct) (((struct NODE *)(NodeStruct))->Name)


/*---------------------------------------------------------------------*
 |                                                                     |
 |    xSucc (NodeStruct) scans for the ->Node.Succ of a node struct    |
 |                                                                     |
 *---------------------------------------------------------------------*/
#define xSucc(NodeStruct) (((struct NODE *)(NodeStruct))->Succ)


/*---------------------------------------------------------------------*
 |                                                                     |
 |    xPred (NodeStruct) scans for the ->Node.Pred of a node struct    |
 |                                                                     |
 *---------------------------------------------------------------------*/
#define xPred(NodeStruct) (((struct NODE *)(NodeStruct))->Pred)


/*---------------------------------------------------------------------*
 |                                                                     |
 |    xForeach(List,NodeStruct) builds a loop to process each list     |
 |    element.                                                         |
 |                                                                     |
 *---------------------------------------------------------------------*/
#define xForeach(List,NodeStruct) \
   if (List) for ((NodeStruct) = (void *) ((struct LIST *)(List))->Head; \
           (NodeStruct); (NodeStruct) = (void *) xSucc (NodeStruct))


/*---------------------------------------------------------------------*
 |                                                                     |
 |    xForeachReverse(List,NodeStruct) builds a loop to process each   |
 |    element in reverse order.                                        |
 |                                                                     |
 *---------------------------------------------------------------------*/
#define xForeachReverse(List,NodeStruct) \
   if (List) for ((NodeStruct) = (void *) ((struct LIST *)(List))->Tail; \
           NodeStruct; (NodeStruct) = (void *) xPred (NodeStruct))


/*---------------------------------------------------------------------*
 |                                                                     |
 |    xRemove(List,NodeStruct) unchains a node struct out of a list.   |
 |                                                                     |
 *---------------------------------------------------------------------*/
#define xRemove(List,NodeStruct) \
   do \
   { \
      struct NODE *TmpNode; \
      struct LIST *TmpList; \
      \
      TmpNode = ((struct NODE *)(NodeStruct)); \
      TmpList = ((struct LIST *)(List)); \
	  \
      if (TmpNode->Pred) \
         (TmpNode->Pred)->Succ = TmpNode->Succ; \
      else TmpList->Head = TmpNode->Succ; \
      if (TmpNode->Succ) \
         (TmpNode->Succ)->Pred = TmpNode->Pred; \
      else TmpList->Tail = TmpNode->Pred; \
      TmpList->Size --; \
   } while (0)


/*************************************************************************
 *                                                                       *
 *     function  :   xAddHead                                            *
 *                                                                       *
 *     arguments :   List - pointer to a LIST structure                  *
 *                                                                       *
 *                   Node - pointer to a NODE structure                  *
 *                                                                       *
 *-----------------------------------------------------------------------*
 *                                                                       *
 *     xAddHead() inserts 'Node' at the head of 'List'.                  *
 *                                                                       *
 *************************************************************************/
#define xAddHead(List, NodeStruct) \
   do { \
      struct NODE *TmpNode; \
      struct LIST *TmpList; \
      \
      TmpNode = ((struct NODE *)(NodeStruct)); \
      TmpList = ((struct LIST *)(List)); \
	  \
      if (TmpList->Head) { \
         TmpNode->Pred = NULL; \
         TmpNode->Succ = TmpList->Head; \
         (TmpList->Head)->Pred = TmpNode; \
         TmpList->Head = TmpNode; } \
      else { \
         TmpList->Head = TmpNode; \
         TmpList->Tail = TmpNode; \
         TmpNode->Pred = NULL; \
         TmpNode->Succ = NULL; } \
      TmpList->Size++; \
   } while (0)


/*************************************************************************
 *                                                                       *
 *     function  :   xAddTail                                            *
 *                                                                       *
 *     arguments :   List - pointer to a LIST structure                  *
 *                                                                       *
 *                   Node - pointer to a NODE structure                  *
 *                                                                       *
 *-----------------------------------------------------------------------*
 *                                                                       *
 *     xAddTail() inserts 'Node' at the tail of 'List'.                  *
 *                                                                       *
 *************************************************************************/
#define xAddTail(List, NodeStruct) \
   do { \
      struct NODE *TmpNode; \
      struct LIST *TmpList; \
      \
      TmpNode = ((struct NODE *)(NodeStruct)); \
      TmpList = ((struct LIST *)(List)); \
	  \
      if (TmpList->Head) { \
         TmpNode->Succ = NULL; \
         TmpNode->Pred = TmpList->Tail; \
         (TmpList->Tail)->Succ = TmpNode; \
         TmpList->Tail = TmpNode; } \
      else { \
         TmpList->Head = TmpNode; \
         TmpList->Tail = TmpNode; \
         TmpNode->Pred = NULL; \
         TmpNode->Succ = NULL; } \
      TmpList->Size++; \
   } while (0)


/*************************************************************************
 *                                                                       *
 *     function  :   xRemHead                                            *
 *                                                                       *
 *     arguments :   List - pointer to a LIST structure                  *
 *                                                                       *
 *-----------------------------------------------------------------------*
 *                                                                       *
 *     xRemHead() removes a Node from head of 'List'.                    *
 *                                                                       *
 *************************************************************************/
#define xRemHead(List) \
   do { \
      struct LIST *TmpList; \
      \
      TmpList = ((struct LIST *)(List)); \
	  \
      if (TmpList->Head) \
      { \
         TmpList->Head = (TmpList->Head)->Succ; \
         if (TmpList->Head) (TmpList->Head)->Pred = NULL; \
         else TmpList->Tail = NULL; \
         TmpList->Size--; \
      } \
   } while (0)


/*************************************************************************
 *                                                                       *
 *     function  :   xRemTail                                            *
 *                                                                       *
 *     arguments :   List - pointer to a LIST structure                  *
 *                                                                       *
 *-----------------------------------------------------------------------*
 *                                                                       *
 *     xRemTail() removes a Node from the tail of 'List'.                *
 *                                                                       *
 *************************************************************************/
#define xRemTail(List) \
   do { \
      struct LIST *TmpList; \
      \
      TmpList = ((struct LIST *)(List)); \
	  \
      if (TmpList->Tail) \
      { \
         TmpList->Tail = (TmpList->Tail)->Pred; \
         if (TmpList->Tail) (TmpList->Tail)->Succ = NULL; \
         else TmpList->Head = NULL; \
         TmpList->Size--; \
      } \
   } while (0)


/*************************************************************************
 *                                                                       *
 *     function  :   xConCat                                             *
 *                                                                       *
 *     arguments :   DestinationList - pointer to the destination        *
 *                                     LIST structure                    *
 *                                                                       *
 *                   SourceList - pointer to the source LIST structure   *
 *                                                                       *
 *-----------------------------------------------------------------------*
 *                                                                       *
 *   xConCat() concats 'SourceList' with 'DestinationList' and clears    *
 *   'SourceList'.                                                       *
 *                                                                       *
 *************************************************************************/
#define xConCat(DestinationList, SourceList) \
   do { \
      struct LIST *SrcList; \
      struct LIST *DstList; \
      \
      SrcList = ((struct LIST *)(SourceList)); \
      DstList = ((struct LIST *)(DestinationList)); \
	  \
      if (DstList && SrcList) \
      { \
         if (DstList->Head) { \
            if (SrcList->Head) { \
               (DstList->Tail)->Succ = SrcList->Head; \
               (SrcList->Head)->Pred = DstList->Tail; \
               DstList->Tail = SrcList->Tail; \
               DstList->Size += SrcList->Size; \
               SrcList->Size = 0; \
               SrcList->Head = NULL; \
               SrcList->Tail = NULL; } } \
         else { \
            DstList->Head = SrcList->Head; \
            DstList->Tail = SrcList->Tail; \
            DstList->Size += SrcList->Size; \
            SrcList->Size = 0; \
            SrcList->Head = NULL; \
            SrcList->Tail = NULL; } \
      } \
      else if (SrcList) ((struct LIST *)(DestinationList)) = SrcList; \
   } while (0)



#define xJoinList(SourceList, DestinationList, NodeStruct) \
   do { \
      struct NODE *KeyNode; \
      struct NODE *TmpNode; \
      struct LIST *SrcList; \
      struct LIST *DstList; \
      \
      KeyNode = ((struct NODE *)(NodeStruct)); \
      SrcList = ((struct LIST *)(SourceList)); \
      DstList = ((struct LIST *)(DestinationList)); \
	  \
      if (SrcList->Head) \
      { \
         TmpNode = KeyNode->Succ; \
         KeyNode->Succ = SrcList->Head; \
         SrcList->Tail->Succ = TmpNode; \
         SrcList->Head->Pred = KeyNode; \
         if (!TmpNode) DstList->Tail = SrcList->Tail; \
         else TmpNode->Pred = SrcList->Tail; \
         DstList->Size += SrcList->Size; \
         SrcList->Size = 0; \
         SrcList->Head = NULL; \
         SrcList->Tail = NULL; \
      } \
   } while (0)

#define xJoin(SourceNode, DestinationList, NodeStruct) \
   do { \
      struct NODE *KeyNode; \
      struct NODE *TmpNode; \
      struct NODE *SrcNode; \
      struct LIST *DstList; \
      \
      KeyNode = ((struct NODE *)(NodeStruct)); \
      SrcNode = ((struct NODE *)(SourceNode)); \
      DstList = ((struct LIST *)(DestinationList)); \
	  \
      if (SrcNode) \
      { \
         TmpNode = KeyNode->Succ; \
         KeyNode->Succ = SrcNode; \
         SrcNode->Succ = TmpNode; \
         SrcNode->Pred = KeyNode; \
         if (!TmpNode) DstList->Tail = SrcNode; \
         else TmpNode->Pred = SrcNode; \
         DstList->Size += 1; \
      } \
   } while (0)

#define xClearList(SrcList) \
   do { \
         (SrcList)->Size = 0; \
         (SrcList)->Head = NULL; \
         (SrcList)->Tail = NULL; \
   } while (0)

#define xSetName(nodestruct, name) \
   do { \
      struct NODE *TmpNode; \
      \
      TmpNode = (struct NODE *) (nodestruct); \
      \
      TmpNode->Name = xSetText (name); \
      TmpNode->HashKey = xHashKey (name); \
   } while (0)

#endif
