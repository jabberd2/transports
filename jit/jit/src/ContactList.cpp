/*
 * ContactList
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

#include <libicq2000/ContactList.h>
#include <libicq2000/events.h>

namespace ICQ2000 {

  ContactList::ContactList() { client = NULL; }

  ContactList::ContactList(const ContactList& cl)
    : m_cmap(cl.m_cmap)
  { client = NULL; }

  ContactRef ContactList::operator[](unsigned int uin)
  {
    return lookup_uin(uin);
  }

  ContactRef ContactList::lookup_uin(unsigned int uin)
  {
    if (m_cmap.count(uin) != 0) return (*(m_cmap.find(uin))).second;
    return NULL;
  }
  
  ContactRef ContactList::lookup_mobile(const std::string& m)
  {
    iterator curr = begin();
    while (curr != end()) {
      if ((*curr)->getNormalisedMobileNo() == m) return (*curr);
      ++curr;
    }

    return NULL;
  }

  ContactRef ContactList::lookup_email(const std::string& em)
  {
    iterator curr = begin();
    while (curr != end()) {
      if ((*curr)->getEmail() == em) return (*curr);
      ++curr;
    }

    return NULL;
  }

  ContactRef ContactList::add(ContactRef ct) {
    m_cmap.insert( std::make_pair(ct->getUIN(), ct) );

    // fire off signal
    UserAddedEvent uev( ct );
    //    contactlist_signal.emit( &uev );
    contactlist_signal_cb(client, signal_type,&uev );

    return ct;
  }

  void ContactList::remove(unsigned int uin) {
    if (m_cmap.count(uin) != 0) {
      // first fire off signal
      UserRemovedEvent uev( m_cmap[uin] );
      //      contactlist_signal.emit( &uev );
      contactlist_signal_cb(client, signal_type,&uev );

      m_cmap.erase(uin);
    }
  }

  bool ContactList::empty() const {
    return m_cmap.empty();
  }

  unsigned int ContactList::size() const {
    return m_cmap.size();
  }

  bool ContactList::exists(unsigned int uin) {
    return (m_cmap.count(uin) != 0);
  }
  
  bool ContactList::mobile_exists(const std::string& m) {
    iterator curr = begin();
    while (curr != end()) {
      if ((*curr)->getNormalisedMobileNo() == m) return true;
      ++curr;
    }
    return false;
  }

  bool ContactList::email_exists(const std::string& em) {
    iterator curr = begin();
    while (curr != end()) {
      if ((*curr)->getEmail() == em) return true;
      ++curr;
    }
    return false;
  }

  ContactList::iterator ContactList::begin() {
    return iterator(m_cmap.begin());
  }

  ContactList::iterator ContactList::end() {
    return iterator(m_cmap.end());
  }

  ContactList::const_iterator ContactList::begin() const {
    return const_iterator(m_cmap.begin());
  }

  ContactList::const_iterator ContactList::end() const {
    return const_iterator(m_cmap.end());
  }

}  
