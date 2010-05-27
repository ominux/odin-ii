#ifndef READ_XML_UTIL_H
#define READ_XML_UTIL_H

#include "util.h"
#include "ezxml.h"

extern ezxml_t FindElement(INP ezxml_t Parent, INP const char *Name, INP boolean Required);
extern void CheckElement(INP ezxml_t Node, INP const char *Name);
extern void FreeNode(INOUTP ezxml_t Node);
extern const char * FindProperty(INP ezxml_t Parent, INP const char *Name, INP boolean);
extern  boolean IsWhitespace(char c);

#endif

