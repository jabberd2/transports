/*
 * SNACs 
 *
 * Copyright (C) 2001 Barnaby Gray <barnaby@beedesign.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 */

#ifndef SNAC_H
#define SNAC_H

#include <libicq2000/buffer.h>

#include <libicq2000/SNAC-base.h>
#include <libicq2000/SNAC-BUD.h>
#include <libicq2000/SNAC-LOC.h>
#include <libicq2000/SNAC-GEN.h>
#include <libicq2000/SNAC-UIN.h>
#include <libicq2000/SNAC-MSG.h>
#include <libicq2000/SNAC-SRV.h>
#include <libicq2000/SNAC-BOS.h>
#include <libicq2000/SNAC-SBL.h>

namespace ICQ2000 {
  InSNAC* ParseSNAC(Buffer& b);
}

#endif
