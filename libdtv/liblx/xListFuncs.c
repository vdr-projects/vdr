//////////////////////////////////////////////////////////////
///                                                        ///
/// xListFuncs.c: list handling functions of liblx         ///
///                                                        ///
//////////////////////////////////////////////////////////////

// $Revision: 1.1 $
// $Date: 2001/06/25 12:29:47 $
// $Author: hakenes $
//
//   (C) 1992-2001 Rolf Hakenes <hakenes@hippomi.de>, under the GNU GPL.
//
// liblx is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// liblx is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You may have received a copy of the GNU General Public License
// along with liblx; see the file COPYING.  If not, write to the
// Free Software Foundation, Inc., 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.

#include "liblx.h"


/*************************************************************************
 *                                                                       *
 *     function  :   xHashKey                                            *
 *                                                                       *
 *     arguments :   Name - character pointer                            *
 *                                                                       *
 *     return    :   16 Bit CRC checksum as hashkey                      *
 *                                                                       *
 *************************************************************************/
unsigned short xHashKey (Name)

char *Name;
{
    unsigned short     Key = 0;
    unsigned long      Value;
    char              *Ptr;

    if (!Name) return (0);

    for (Ptr = Name; *Ptr; Ptr++) {
        Value = ((Key >> 8) ^ (*Ptr)) & 0xFF;
        Value = Value ^ (Value >> 4);
        Key = 0xFFFF & ((Key << 8) ^ Value ^ (Value << 5) ^ (Value << 12));
    }
    return (Key);
}


/*************************************************************************
 *                                                                       *
 *     function  :   xNewNode                                            *
 *                                                                       *
 *     arguments :   Name - character pointer to the node's name         *
 *                                                                       *
 *                   Size - size of the surrounding structure in bytes   *
 *                                                                       *
 *     return    :   pointer to a correct initialized NODE structure     *
 *                                                                       *
 *-----------------------------------------------------------------------*
 *                                                                       *
 *   xNewNode() allocates memory for a NODE structure and initializes    *
 *   it properly. If argument Name points to a string, it copies that    *
 *   into a new allocated memory area and assigns Node->Name to it.      *
 *   Because NODE's are often part of bigger structures, the size of     *
 *   the surrounding structure could be specified to allocate it.        *
 *                                                                       *
 *************************************************************************/

struct NODE *xNewNode (Name, Size)

char *Name;
unsigned long Size;
{
   struct NODE      *Node;

   if (Size < sizeof(struct NODE)) Size = sizeof(struct NODE);

   xMemAlloc (Size, &Node);

   Node->Succ = NULL;
   Node->Pred = NULL;

   if (Name == NULL)
   {
      Node->Name = NULL;
      Node->HashKey = 0;
   }
   else
   {
      xMemAlloc (strlen (Name) + 1, &(Node->Name));
      strcpy (Node->Name, Name);
      Node->HashKey = xHashKey (Name);
   }

   return (Node);
}


/*************************************************************************
 *                                                                       *
 *     function  :   xNewList                                            *
 *                                                                       *
 *     arguments :   Name - character pointer to the list's name         *
 *                                                                       *
 *     return    :   pointer to a correct initialized LIST structure     *
 *                                                                       *
 *-----------------------------------------------------------------------*
 *                                                                       *
 *   xNewList() allocates memory for a LIST structure and initializes    *
 *   it properly. If argument Name points to a string, it copies that    *
 *   into a new allocated memory area and assigns List->Name to it.      *
 *                                                                       *
 *************************************************************************/

struct LIST *xNewList (Name)

char *Name;
{
   struct LIST      *List;

   xMemAlloc (sizeof(struct LIST), &List);

   List->Head = NULL;
   List->Tail = NULL;
   List->Size = 0;

   if (Name == NULL)
   {
      List->Name = NULL;
   }
   else
   {
      xMemAlloc (strlen (Name) + 1, &(List->Name));
      strcpy (List->Name, Name);
   }

   return (List);
}



/*************************************************************************
 *                                                                       *
 *     function  :   xFindName                                           *
 *                                                                       *
 *     arguments :   List - pointer to a LIST structure                  *
 *                                                                       *
 *                   Name - pointer to a name string                     *
 *                                                                       *
 *     return    :   pointer to a NODE structure                         *
 *                                                                       *
 *-----------------------------------------------------------------------*
 *                                                                       *
 *   xFindName() looks for element with name 'Name' in list 'List' and   *
 *   returns its NODE structure.                                         *
 *                                                                       *
 *************************************************************************/

struct NODE *xFindName (List, Name)

struct LIST *List;
char *Name;
{
   struct NODE     *Node;
   unsigned short   HashKey;

   if (!Name || !List) return (NULL);

   HashKey = xHashKey (Name);

   for (Node = List->Head; Node; Node = Node->Succ)
      if (HashKey == Node->HashKey)
         if (Node->Name)
            if (strcmp (Node->Name, Name) == 0) return (Node);

   return (NULL);
}
