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

#include <libicq2000/userinfohelpers.h>

#include "sstream_fix.h"

using std::vector;
using std::string;
using std::ostringstream;
using std::istringstream;

namespace ICQ2000
{
  namespace UserInfoHelpers
  { 
    string getSexIDtoString(Sex id) 
    {
      string ret;
      
      switch(id) {
      case SEX_MALE:
	ret = "Male";
	break;
      case SEX_FEMALE:
	ret = "Female";
	break;
      case SEX_UNSPECIFIED:
      default:
	ret = "Unspecified";
	break;
      }
      return ret;
    }
      
    Sex getSexStringtoID(const string& s)
    {
      if (s == getSexIDtoString(SEX_MALE)) {
	return SEX_MALE;
      } else if (s == getSexIDtoString(SEX_FEMALE)) {
	return SEX_FEMALE;
      } else {
	return SEX_UNSPECIFIED;
      }
    }

    vector<string> getSexAllStrings()
    {
      vector<string> ret;
      ret.push_back( getSexIDtoString(SEX_MALE) );
      ret.push_back( getSexIDtoString(SEX_FEMALE) );
      ret.push_back( getSexIDtoString(SEX_UNSPECIFIED) );
      return ret;
    }
    
    string getTimezoneIDtoString(signed char id)
    {
      if (id > 24 || id < -24) {
	return "Unspecified";
      } else {
	ostringstream ostr;
	ostr << "GMT " << (id > 0 ? "-" : "+")
	     << abs(id/2)
	     << ":"
	     << (id % 2 == 0 ? "00" : "30");
	return ostr.str();
      }
    }
      
    signed char getTimezoneStringtoID(const string& s)
    {
      string t;
      char c1, c2;
      int h, m;
      
      istringstream istr(s);
      if ((istr >> t >> c1 >> h >> c2 >> m)
	  && t == "GMT" && (c1 == '+' || c1 == '-')
	  && h <= 24 && (m == 30 || m == 0)) {
	if (c1 == '+') {
	  return -(h * 2 + (m == 30 ? 1 : 0));
	} else {
	  return h * 2 + (m == 30 ? 1 : 0);
	}
      } else {
	return Timezone_unknown;
      }
    }
    
    vector<string> getTimezoneAllStrings()
    {
      vector<string> ret;
      ret.push_back( getTimezoneIDtoString( Timezone_unknown ) );
      for (signed char n =  -24; n <= 24; ++n) {
	ret.push_back( getTimezoneIDtoString( n ) );
      }
      return ret;
    }
    
    string getTimezonetoLocaltime(signed char id)
    {
      string r;

      if(id <= 24 && id >= -24) {
	time_t rt = time(0) + getSystemTimezone()*1800;
	rt -= id*1800;
	r = ctime(&rt);
      }

      return r;
    }

    signed char getSystemTimezone()
    {
      time_t t = time(NULL);
      struct tm *tzone = localtime(&t);
      int nTimezone = 0;

#ifdef HAVE_TM_ZONE
      nTimezone = -(tzone->tm_gmtoff);
#else
      nTimezone = timezone;
#endif

      nTimezone += (tzone->tm_isdst == 1 ? 3600 : 0);
      nTimezone /= 1800;

      if(nTimezone > 23) return 23-nTimezone;

      return (signed char) nTimezone;
    }
    
    string getLanguageIDtoString(unsigned char id)
    {
      if (id >= Language_table_size) {
	return Language_table[0];
      } else {
	return Language_table[id];
      }
    }
    
    unsigned char getLanguageStringtoID(const string& s)
    {
      for (int n = 0; n < Language_table_size; ++n) {
	if (s == Language_table[n]) return n;
      }
      return 0;
    }

    vector<string> getLanguageAllStrings()
    {
      vector<string> ret;
      for (int n = 0; n < Language_table_size; ++n) {
	ret.push_back( Language_table[n] );
      }
      return ret;
    }
    
    string getCountryIDtoString(unsigned short id)
    {
      for (int n = 0; n < Country_table_size; ++n) {
	if (Country_table[n].code == id) {
	  return Country_table[n].name;
	}
      }
      
      return Country_table[0].name;
    }
    
    unsigned short getCountryStringtoID(const string& s)
    {
      for (int n = 0; n < Country_table_size; ++n) {
	if (s == Country_table[n].name) {
	  return Country_table[n].code;
	}
      }
      
      return Country_table[0].code;
    }

    vector<string> getCountryAllStrings()
    {
      vector<string> ret;
      for (int n = 0; n < Country_table_size; ++n) {
	ret.push_back( Country_table[n].name );
      }
      return ret;
    }
    
    string getInterestsIDtoString(unsigned char id)
    {
      if (id-Interests_offset >= 0 && id-Interests_offset < Interests_table_size) {
	return Interests_table[id-Interests_offset];
      }

      return "";
    }
    
    unsigned char getInterestsStringtoID(const string& s)
    {
      for (int n = 0; n < Interests_table_size; ++n) {
	if (s == Interests_table[n]) {
	  return n+Interests_offset;
	}
      }
    
      return 0;
    }

    vector<string> getInterestsAllStrings()
    {
      vector<string> ret;
      for (int n = 0; n < Interests_table_size; ++n) {
	ret.push_back( Interests_table[n] );
      }
      return ret;
    }
    
    string getBackgroundIDtoString(unsigned short id)
    {
      for (int n = 0; n < Background_table_size; ++n) {
	if (id == Background_table[n].code) {
	  return Background_table[n].name;
	}
      }
      
      return "";
    }
    
    unsigned short getBackgroundStringtoID(const string& s)
    {
      for (int n = 0; n < Background_table_size; ++n) {
	if (s == Background_table[n].name) {
	  return Background_table[n].code;
	}
      }
      
      return 0;
    }

    vector<string> getBackgroundAllStrings()
    {
      vector<string> ret;
      for (int n = 0; n < Background_table_size; ++n) {
	ret.push_back( Background_table[n].name );
      }
      return ret;
    }
    
  }
}
