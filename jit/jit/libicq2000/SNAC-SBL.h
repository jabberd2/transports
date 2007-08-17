/*
 * SNAC - Server-based list management
 * Mitz Pettel, 2001
 *
 * based on: SNAC - Buddy (Contact) list management
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

#ifndef SNAC_SBL_H
#define SNAC_SBL_H

#include <string>
#include <list>

#include <libicq2000/SNAC-base.h>
#include <libicq2000/Contact.h>
#include <libicq2000/ContactList.h>
#include <libicq2000/UserInfoBlock.h>

namespace ICQ2000 {

  // Server-based list stuff (Family 0x0013)
  const unsigned short SNAC_SBL_Request_List = 0x0005;
  const unsigned short SNAC_SBL_List_From_Server = 0x0006;

  // ----------------- Server-based Lists (Family 0x0013) SNACs -----------

  class SBLFamilySNAC : virtual public SNAC {
   public:
    unsigned short Family() const { return SNAC_FAM_SBL; }
  };

  class RequestSBLSNAC : public SBLFamilySNAC, public OutSNAC {
    
   protected:
    void OutputBody(Buffer& b) const;

   public:
    RequestSBLSNAC();

    unsigned short Subtype() const { return SNAC_SBL_Request_List; }
  };
  
  class SBLListSNAC : public SBLFamilySNAC, public InSNAC {
   private:
    ContactList m_contacts;
     
   protected:
    void ParseBody(Buffer& b);

   public:
    SBLListSNAC();
    
    const ContactList& getContactList() const { return m_contacts; }
    unsigned short Subtype() const { return SNAC_SBL_List_From_Server; }
  };
  
}

#endif
