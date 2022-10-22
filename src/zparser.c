#include <zstd.h>
#include <zparser.h>

znode_t znode_create(ztok_t tok)
{
    znode_t node;
    node.token = tok;
    node.children = array_create(sizeof(znode_t));
    return node;
}

void znode_free(znode_t* node)
{
    size_t i;
    znode_t* child = node->children.data;
    const size_t count = node->children.size;
    for (i = 0; i < count; ++i) {
        znode_free(child++);
    }
    array_free(&node->children);
}

void znode_connect(znode_t* parent, const znode_t* child)
{
    array_push(&parent->children, child);
}

int zcc_parse(const char* str)
{
    (void)str;
    return ZCC_EXIT_SUCCESS;
}