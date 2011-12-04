/**********************************************************************
 *
 * HDFF firmware command interface library
 *
 * Copyright (C) 2011  Andreas Regel
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *********************************************************************/

#ifndef HDFFCMD_GENERIC_H
#define HDFFCMD_GENERIC_H

int HdffCmdGetFirmwareVersion(int OsdDevice, uint32_t * Version, char * String,
                              uint32_t MaxLength);

int HdffCmdGetInterfaceVersion(int OsdDevice, uint32_t * Version, char * String,
                               uint32_t MaxLength);

int HdffCmdGetCopyrights(int OsdDevice, uint8_t Index, char * String,
                         uint32_t MaxLength);

#endif /* HDFFCMD_GENERIC_H */
