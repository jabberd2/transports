/* $Id: vcard_photo.h,2008/02/19 15:14:07 smoku Exp $ */

/*
 *  (C) Copyright 2008 Tomasz Sterna
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef vcard_photo_h
#define vcard_photo_h

#define	VCARD_PHOTO_TYPE	"image/png"
#define VCARD_PHOTO_BINVAL	"\
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\n\
RK5CYII="

#endif
