#include <string.h>
#include "xmlbuf.h"

xmlDocPtr xmlbuf_new_doc_from_buffer(const char *membuf) {
	xmlDocPtr doc = NULL;
	int size = strlen(membuf);

	doc = xmlReadMemory(membuf, size, "proto.xml", NULL, 0);
	return doc;
}

xmlDocPtr xmlbuf_new_doc_from_file(const char *filename) {
	xmlDocPtr doc = NULL;

	return xmlReadFile(filename, NULL, XML_PARSE_NOBLANKS);
}

xmlNodePtr xmlbuf_new_node(xmlNodePtr parent, char *tag, char *content) {
	xmlNodePtr node = xmlNewChild(parent, NULL, tag, content);
	return node;
}

void xmlbuf_new_prop(xmlNodePtr node, char *attr, char *val) {
	xmlNewProp(node, attr, val);
}

void xmlbuf_set_prop(xmlNodePtr node, char *name, char *val) {
	xmlSetProp(node, name, val);
}

char* xmlbuf_get_prop(xmlNodePtr node, char *name) {
	return xmlGetProp(node, name);
}

void xmlbuf_set_content(xmlNodePtr node, char *content) {
	xmlNodeSetContent(node, content);
}

char *xmlbuf_get_content(xmlNodePtr node) {
	return xmlNodeGetContent(node);
}

void xmlbuf_node_foreach(xmlNodePtr node, node_foreach_t nodecb, void *data) {
	xmlNode *ptr = NULL;

	for(ptr = xmlFirstElementChild(node); ptr; ptr = ptr->next) {
		if(ptr->type == XML_ELEMENT_NODE)
			(* nodecb)(ptr, data);
	}
}

void xmlbuf_nodeset_foreach(xmlNodeSetPtr nodes, node_foreach_t nodecb, void *data) {
	int size = 0, i;

	size = nodes ? nodes->nodeNr : 0;

	for(i=size-1; i>=0; i--) {
		(* nodecb)(nodes->nodeTab[i], data);
	}
}

xmlNodeSetPtr xmlbuf_new_xpath(xmlDocPtr doc, char *xpath) {
	xmlXPathContextPtr ctx = NULL;
	xmlXPathObjectPtr obj = NULL;

	ctx = xmlXPathNewContext(doc);
	if(ctx == NULL)
		return NULL;

	obj = xmlXPathEvalExpression(xpath, ctx);
	if(obj == NULL) {
		return NULL;
	}

	return obj->nodesetval;
}

void xmlbuf_remove_node(xmlNodePtr node) {
	xmlUnlinkNode(node);
	xmlFreeNode(node);
}

void xmlbuf_save_to_file(char *path, xmlDocPtr doc) {
	xmlSaveFormatFileEnc(path, doc, "UTF-8", 0);
}

xmlNodePtr xmlbuf_get_root(xmlDocPtr doc) {
	return xmlDocGetRootElement(doc);
}

void xmlbuf_free_doc(xmlDocPtr doc) {
	xmlFreeDoc(doc);
}

void xmlbuf_cleanup() {
	xmlCleanupParser();
}
