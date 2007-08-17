/*
 * SNAC - Server-based lists
 * Mitz Pettel, 2001
 *
 * based on: SNAC - Buddy List
 *
 * Copyright (C) 2001 Barnaby Gray <barnaby@beedesign.co.uk>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <libicq2000/SNAC-SBL.h>

#include <libicq2000/TLV.h>

using std::string;

namespace ICQ2000 {

  // --------------- Server-based Lists (Family 0x0013) SNACs --------------

  RequestSBLSNAC::RequestSBLSNAC() { }

  void RequestSBLSNAC::OutputBody(Buffer& b) const {
      b << (unsigned int)(0);
      b << (unsigned short)(1);
    }

  SBLListSNAC::SBLListSNAC() { }
  
  void SBLListSNAC::ParseBody(Buffer& b) {
        
        b.advance(1);			// 00
        unsigned short entityCount;
        b >> entityCount;		// number of entities?
        while (b.pos()<=b.size()-10)
        {
            unsigned short nameLength;
            b >> nameLength;
            string name;
            b.Unpack(name, nameLength);
            b.advance(6);
            unsigned short dataLength;
            b >> dataLength;
            while (dataLength>=2)
            {
                unsigned short infoType;
                unsigned short infoLength;
                b >> infoType;
                dataLength -= 2;
                b >> infoLength;
                dataLength -= 2;
                if (infoType==0x0131)	// UIN
                {
                    ContactRef c(new Contact(Contact::StringtoUIN(name)));
                    string nickname;
                    b.Unpack(nickname, infoLength);
                    dataLength -= infoLength;
                    c->setAlias(nickname);
                    m_contacts.add(c);
                    break;
                }
                else			// other stuff we don't understand
                {
                    b.advance(infoLength);
                    dataLength -= infoLength;
                }
            }
            b.advance(dataLength);
        }
        b.advance(4);
    }
  }
