#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "ezxml.h"
#include "read_xml_util.h"

/* Finds child element with given name and returns it. Errors out if
 * more than one instance exists. */
ezxml_t
FindElement(INP ezxml_t Parent, INP const char *Name, INP boolean Required)
{
    ezxml_t Cur;

        /* Find the first node of correct name */
        Cur = ezxml_child(Parent, Name);

        /* Error out if node isn't found but they required it */
        if(Required)
        {
            if(NULL == Cur)
                {
                    printf(ERRTAG
                            "Element '%s' not found within element '%s'.\n",
                            Name, Parent->name);
                    exit(1);
                }
        }

        /* Look at next tag with same name and error out if exists */
        if(Cur != NULL && Cur->next)
        {
            printf(ERRTAG "Element '%s' found twice within element '%s'.\n",
                    Name, Parent->name);
            exit(1);
        }
    return Cur;
}

/* Checks the node is an element with name equal to one given */
void
CheckElement(INP ezxml_t Node, INP const char *Name)
{
    if(0 != strcmp(Node->name, Name))
        {
            printf(ERRTAG
                    "Element '%s' within element '%s' does match expected "
                    "element type of '%s'\n", Node->name, Node->parent->name,
                    Name);
            exit(1);
        }
}

/* Checks that the node has no remaining child nodes or property nodes,
 * then unlinks the node from the tree which updates pointers and
 * frees the memory */
void
FreeNode(INOUTP ezxml_t Node)
{
    ezxml_t Cur;
    char *Txt;


        /* Shouldn't have unprocessed properties */
        if(Node->attr[0])
        {
            printf(ERRTAG "Node '%s' has invalid property %s=\"%s\".\n",
                    Node->name, Node->attr[0], Node->attr[1]);
            exit(1);
        }

        /* Shouldn't have non-whitespace text */
        Txt = Node->txt;
    while(*Txt)
        {
            if(!IsWhitespace(*Txt))
                {
                    printf(ERRTAG
                            "Node '%s' has unexpected text '%s' within it.\n",
                            Node->name, Node->txt);
                    exit(1);
                }
            ++Txt;
        }

        /* We shouldn't have child items left */
        Cur = Node->child;
    if(Cur)
        {
            printf(ERRTAG "Node '%s' has invalid child node '%s'.\n",
                    Node->name, Cur->name);
            exit(1);
        }

        /* Now actually unlink and free the node */
        ezxml_remove(Node);
}

/* Returns TRUE if character is whatspace between tokens */
boolean
IsWhitespace(char c)
{
    switch (c)
        {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            return TRUE;
        default:
            return FALSE;
        }
}

const char *
FindProperty(INP ezxml_t Parent, INP const char *Name, INP boolean Required)
{
    const char *Res;

    Res = ezxml_attr(Parent, Name);
    if(Required)
        {
            if(NULL == Res)
                {
                    printf(ERRTAG
                            "Required property '%s' not found for element '%s'.\n",
                            Name, Parent->name);
                    exit(1);
                }
        }
    return Res;
}


