/*
 * Translate class
 *
 * Many parts of this source code were 'inspired' by the ircII4.4 translat.c source.
 * RIPPED FROM KVirc: http://www.kvirc.org
 * Original by Szymon Stefanek (kvirc@tin.it).
 * Modified by Andrew Frolov (dron@linuxer.net)
 * Further modified by Graham Roff
 *
 * 'Borrowed' from licq - thanks Barnaby
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

#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#include <string>
#include <exception>

namespace ICQ2000 {
  class TranslatorException : public std::exception {
   private:
    std::string m_errortext;
    
   public:
    TranslatorException(const std::string& text);
    ~TranslatorException() throw() { }
    
    const char* what() const throw();
  };

  class Translator{
   public:
    Translator();
    void setDefaultTranslationMap();
    void setTranslationMap(const std::string& szMapFileName);
    void ServerToClient(std::string& szString);
    void ClientToServer(std::string& szString);
    std::string ServerToClientCC(const std::string& szString);
    std::string ClientToServerCC(const std::string& szString);
    void ServerToClient(char &_cChar);
    void ClientToServer(char &_cChar);
    static void CRLFtoLF(std::string& s);
    static void LFtoCRLF(std::string& s);
    bool usingDefaultMap() const { return m_bDefault; }
    const std::string& getMapFileName() const { return m_szMapFileName; }
    const std::string& getMapName() const { return m_szMapName; }

   protected:
    unsigned char serverToClientTab[256];
    unsigned char clientToServerTab[256];
    std::string m_szMapFileName, m_szMapName;
    bool m_bDefault;
  };
}

#endif
