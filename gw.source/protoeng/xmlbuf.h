#ifndef __XMLBUF_H__
#define __XMLBUF_H__
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

typedef void (* node_foreach_t)(xmlNodePtr node, void *data);

xmlDocPtr xmlbuf_new_doc_from_buffer(const char *membuf);
xmlDocPtr xmlbuf_new_doc_from_file(const char *filename);

xmlNodePtr xmlbuf_new_node(xmlNodePtr parent, char *tag, char *content);
void xmlbuf_new_prop(xmlNodePtr node, char *attr, char *val);
xmlNodeSetPtr xmlbuf_new_xpath(xmlDocPtr doc, char *xpath);

void xmlbuf_remove_node(xmlNodePtr node);

void xmlbuf_set_prop(xmlNodePtr node, char *name, char *val);
char* xmlbuf_get_prop(xmlNodePtr node, char *name);
void xmlbuf_set_content(xmlNodePtr node, char *content);
char *xmlbuf_get_content(xmlNodePtr node);

void xmlbuf_node_foreach(xmlNodePtr node, node_foreach_t nodecb, void *data);
void xmlbuf_nodeset_foreach(xmlNodeSetPtr nodes, node_foreach_t nodecb, void *data);


void xmlbuf_save_to_file(char *path, xmlDocPtr doc);
xmlNodePtr xmlbuf_get_root(xmlDocPtr doc);
void xmlbuf_free_doc(xmlDocPtr doc);
void xmlbuf_cleanup();

#endif
