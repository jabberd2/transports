/*
 * Translate class
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

#include <string>
#include <fstream>
#include "sstream_fix.h"

#include <libicq2000/Translator.h>

using std::ifstream;
using std::string;
using std::istringstream;

namespace ICQ2000 {
  //============ Translator ============//

  Translator::Translator() {
    setDefaultTranslationMap();
  }

  //============ setDefaultTranslationMap ============//

  void Translator::setDefaultTranslationMap(){
    for(int i = 0; i < 256; i++){
      serverToClientTab[i]=i;
      clientToServerTab[i]=i;
    }

    m_bDefault = true;
    m_szMapFileName = "none";
    m_szMapName = "none";

  }

  //============ setTranslationMap ============//

  void Translator::setTranslationMap(const string& _szMapFileName){
    // Map name is the file name with no path
    string::size_type pos = _szMapFileName.rfind('/');
    if ( pos == string::npos)
      m_szMapName = _szMapFileName;
    else
      m_szMapName = string(_szMapFileName,pos+1);


    if (m_szMapName == "" || _szMapFileName == "none") {
      setDefaultTranslationMap();
      return;
    }

    ifstream mapFile(_szMapFileName.c_str());
    if (!mapFile) {
      setDefaultTranslationMap();
      throw TranslatorException("Could not open the translation file for reading");
    }

    char buffer[80];
    int inputs[8];
    unsigned char temp_table[512];
    int c = 0;
    char skip;      
    while(mapFile.getline(buffer, 80) != NULL && c < 512){

      istringstream istr(buffer);
      istr.setf(std::ios::hex,std::ios::basefield);
      istr>>skip>>skip>>inputs[0]
	  >>skip>>skip>>skip>>inputs[1]
	  >>skip>>skip>>skip>>inputs[2]
	  >>skip>>skip>>skip>>inputs[3]
	  >>skip>>skip>>skip>>inputs[4]
	  >>skip>>skip>>skip>>inputs[5]
	  >>skip>>skip>>skip>>inputs[6]
	  >>skip>>skip>>skip>>inputs[7];

      if(istr.fail()) {
	setDefaultTranslationMap();
	mapFile.close();
	throw TranslatorException("Syntax error in translation file");
      }

      for (int j = 0; j < 8; j++)
	temp_table[c++] = (unsigned char)inputs[j];

    }

    mapFile.close();

    if (c == 512) {
      for (c = 0; c < 256; c++){
	serverToClientTab[c] = temp_table[c];
	clientToServerTab[c] = temp_table[c | 256];
      }
    } else {
      setDefaultTranslationMap();
      throw TranslatorException("Translation file "+_szMapFileName+" corrupted.");
    }
  
    m_bDefault = false;
    m_szMapFileName = _szMapFileName;
  }

  //============ translateToClient ============//

  void Translator::ServerToClient(string& szString){
    CRLFtoLF(szString);  
    if (m_bDefault)
      return;
    int len=szString.length();
    for(int i=0;i<len;i++)
      szString[i]=serverToClientTab[(unsigned char)szString[i]];
  }

  string Translator::ServerToClientCC(const string& s)
  {
    string cc = s;
    ServerToClient(cc);
    return cc;
  }

  //============ translateToServer ============//

  void Translator::ClientToServer(string& szString){
    LFtoCRLF(szString);
    if (m_bDefault)
      return;
    int len=szString.length();
    for(int i=0;i<len;i++)
      szString[i]=clientToServerTab[(unsigned char)szString[i]];
  }

  string Translator::ClientToServerCC(const string& s)
  {
    string cc = s;
    ClientToServer(cc);
    return cc;
  }

  //-----translateToClient (char)-------------------------------------------------
  void Translator::ServerToClient(char &_cChar){
    if (m_bDefault) return;
    _cChar = serverToClientTab[(unsigned char)(_cChar)];
  }


  //-----translateToServer (char)-------------------------------------------------
  void Translator::ClientToServer(char &_cChar){
    if (m_bDefault) return;
    _cChar = clientToServerTab[(unsigned char)(_cChar)];
  }

  void Translator::LFtoCRLF(string& s) {
    int curr = 0, next;
    while ( (next = s.find( "\n", curr )) != -1 ) {
      s.replace( next, 1, "\r\n" );
      curr = next + 2;
    }
  }

  void Translator::CRLFtoLF(string& s) {
    int curr = 0, next;
    while ( (next = s.find( "\r\n", curr )) != -1 ) {
      s.replace( next, 2, "\n" );
      curr = next + 1;
    }
  }

  /**
   * TranslatorException class
   */
  TranslatorException::TranslatorException(const string& text) : m_errortext(text) { }

  const char* TranslatorException::what() const throw() {
    return m_errortext.c_str();
  }
}
