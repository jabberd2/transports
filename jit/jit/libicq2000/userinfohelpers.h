/*
 * User Info Helpers
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

#ifndef USERINFOHELPERS_H
#define USERINFOHELPERS_H

#include <libicq2000/userinfoconstants.h>

#include <string>
#include <vector>

#include <time.h>

namespace ICQ2000
{

  namespace UserInfoHelpers
  {
    std::string getSexIDtoString(Sex id);
    Sex getSexStringtoID(const std::string& s);
    std::vector<std::string> getSexAllStrings();
    
    std::string getTimezoneIDtoString(signed char id);
    signed char getTimezoneStringtoID(const std::string& s);
    std::vector<std::string> getTimezoneAllStrings();
    std::string getTimezonetoLocaltime(signed char id);
    signed char getSystemTimezone();

    std::string getLanguageIDtoString(unsigned char id);
    unsigned char getLanguageStringtoID(const std::string& s);
    std::vector<std::string> getLanguageAllStrings();

    std::string getCountryIDtoString(unsigned short id);
    unsigned short getCountryStringtoID(const std::string& s);
    std::vector<std::string> getCountryAllStrings();

    std::string getInterestsIDtoString(unsigned char id);
    unsigned char getInterestsStringtoID(const std::string& s);
    std::vector<std::string> getInterestsAllStrings();
    
    std::string getBackgroundIDtoString(unsigned short id);
    unsigned short getBackgroundStringtoID(const std::string& s);
    std::vector<std::string> getBackgroundAllStrings();
  }
  
}

#endif
