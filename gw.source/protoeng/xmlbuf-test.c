#include "xmlbuf.h"

void node_foreach_dump(xmlNodePtr node, void *data) {
	printf("node.val = %s\n", xmlbuf_get_prop(node, "val"));
	xmlUnlinkNode(node);
}

int main(int argc, char *argv[]) {
	xmlDocPtr doc = NULL;
	xmlNodeSetPtr node = NULL;

	doc = xmlbuf_new_doc_from_file("test.xml");
	node = xmlbuf_new_xpath(doc, "//tag");
	xmlbuf_nodeset_foreach(node, node_foreach_dump, NULL);
	xmlbuf_save_to_file("test01.xml", doc);
	xmlbuf_free_doc(doc);
	xmlbuf_cleanup();

	return 0;
}
