/*
 * XML Parser/Generator
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

#include <libicq2000/Xml.h>

using std::string;
using std::list;

// ---------- XmlNode ---------------------------

XmlNode::XmlNode(const string& t) : tag(t) { }

XmlNode::~XmlNode() { }

bool XmlNode::isLeaf() { return !isBranch(); }

string XmlNode::getTag() { return tag; }

string XmlNode::replace_all(const string& s, const string& r1, const string& r2) {
  string t(s);
  int curr = 0, next;
  while ( (next = t.find( r1, curr )) != -1) {
    t.replace( next, r1.size(), r2 );
    curr = next + r2.size();
  }
  return t;
}

string XmlNode::quote(const string& a) {
  return 
    replace_all(
    replace_all(
    replace_all(a,
    "&", "&amp;"),
    "<", "&lt;"),
    ">", "&gt;");
}

string XmlNode::unquote(const string& a) {
  return 
    replace_all(
    replace_all(
    replace_all(a,
    "&lt;", "<"),
    "&gt;", ">"),
    "&amp;", "&");
}

XmlNode *XmlNode::parse(string::iterator& curr, string::iterator end) {

  skipWS(curr,end);
  if (curr == end || *curr != '<') return NULL;

  string tag = parseTag(curr,end);
  if (tag.empty() || tag[0] == '/') return NULL;

  skipWS(curr,end);
  if (curr == end) return NULL;
  
  if (*curr == '<') {

    XmlNode *p = NULL;
    while (curr != end) {
      string::iterator mark = curr;
      string nexttag = parseTag(curr,end);
      if (nexttag.empty()) { if (p != NULL) delete p; return NULL; }
      if (nexttag[0] == '/') {
	// should be the closing </tag>
	if (nexttag.size() == tag.size()+1 && nexttag.find(tag,1) == 1) {
	  // is closing tag
	  if (p == NULL) p = new XmlLeaf(unquote(tag),"");
	  return p;
	} else {
	  if (p != NULL) delete p;
	  return NULL;
	}
      } else {
	if (p == NULL) p = new XmlBranch(unquote(tag));
	// an opening tag
	curr = mark;
	XmlNode *c = parse(curr,end);
	if (c != NULL) ((XmlBranch*)p)->pushnode(c);
      }
      skipWS(curr,end);
      if(curr == end || *curr != '<') {
	if (p != NULL) delete p;
	return NULL;
      }
    }

  } else {
    // XmlLeaf
    string value;
    while (curr != end && *curr != '<') {
      value += *curr;
      curr++;
    }
    if(curr == end) return NULL;
    string nexttag = parseTag(curr,end);
    if (nexttag.empty() || nexttag[0] != '/') return NULL;
    if (nexttag.size() == tag.size()+1 && nexttag.find(tag,1) == 1) {
      return new XmlLeaf(unquote(tag),unquote(value));
    } else {
      // error
      return NULL;
    }
  }
  
  // should never get here
  return NULL;
}

string XmlNode::parseTag(string::iterator& curr, string::iterator end) {
  string tag;
  if(curr == end || *curr != '<') return string();
  curr++;
  while(curr != end && *curr != '>') {
    tag += *curr;
    curr++;
  }
  if (curr == end) return string();
  curr++;
  return tag;
}

void XmlNode::skipWS(string::iterator& curr, string::iterator end) {
  while(curr != end && isspace(*curr)) curr++;
}

// ----------- XmlBranch ------------------------

XmlBranch::XmlBranch(const string& t) : XmlNode(t) { }

XmlBranch::~XmlBranch() {
  list<XmlNode*>::iterator curr = children.begin();
  while (curr != children.end()) {
    delete (*curr);
    curr++;
  }
  children.clear();
}

bool XmlBranch::isBranch() { return true; }

bool XmlBranch::exists(const string& tag) {
  list<XmlNode*>::iterator curr = children.begin();
  while (curr != children.end()) {
    if ((*curr)->getTag() == tag) return true;
    curr++;
  }
  return false;
}

XmlNode *XmlBranch::getNode(const string& tag) {
  list<XmlNode*>::iterator curr = children.begin();
  while (curr != children.end()) {
    if ((*curr)->getTag() == tag) return (*curr);
    curr++;
  }
  return NULL;
}

XmlBranch *XmlBranch::getBranch(const string& tag) {
  XmlNode *t = getNode(tag);
  if (t == NULL || dynamic_cast<XmlBranch*>(t) == NULL) return NULL;
  return dynamic_cast<XmlBranch*>(t);
}

XmlLeaf *XmlBranch::getLeaf(const string& tag) {
  XmlNode *t = getNode(tag);
  if (t == NULL || dynamic_cast<XmlLeaf*>(t) == NULL) return NULL;
  return dynamic_cast<XmlLeaf*>(t);
}

void XmlBranch::pushnode(XmlNode *c) {
  children.push_back(c);
}

string XmlBranch::toString(int n) {
  string ret(n,'\t');
  ret += "<" + quote(tag) + ">\n";
  list<XmlNode*>::iterator curr = children.begin();
  while (curr != children.end()) {
    ret += (*curr)->toString(n+1);
    curr++;
  }
  ret += string(n,'\t') + "</" + quote(tag) + ">\n";

  return ret;
}

// ----------- XmlLeaf --------------------------

XmlLeaf::XmlLeaf(const string& t, const string& v)
  : XmlNode(t), value(v) { }

XmlLeaf::~XmlLeaf() { }

bool XmlLeaf::isBranch() { return false; }

string XmlLeaf::getValue() { return value; }

string XmlLeaf::toString(int n) {
  return string(n,'\t') + "<" + quote(tag) + ">" + quote(value) + "</" + quote(tag) + ">\n";
}

