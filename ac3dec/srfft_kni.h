/*****
*
* This file is part of the OMS program.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by 
* the Free Software Foundation; either version 2, or (at your option)
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; see the file COPYING.  If not, write to
* the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*
*****/

#ifndef __SRFF_KNI__
#define __SRFF_KNI__

#include "cmplx.h"

void fft_4_kni (complex_t *a);
void fft_8_kni (complex_t *a);
void fft_asmb_kni (int, complex_t*, complex_t *, complex_t *, complex_t*);

#endif
