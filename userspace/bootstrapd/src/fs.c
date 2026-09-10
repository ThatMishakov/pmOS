#include "fs.h"
#include <alloca.h>
#include <pmos/system.h>
#include <pmos/ipc.h>
#include "io.h"
#include "module.h"
#include <string.h>
#include <pmos/pmbus_object.h>
#include <pmos/helpers.h>
#include <errno.h>
#include <inttypes.h>
#include <pmos/memory.h>
#include <sys/mman.h>
#include <sys/stat.h>

extern pmos_right_t posix_server_right;
extern uint64_t loader_port;
extern pmos_port_t request_port;

extern struct pmos_msgloop_data msgloop_data;
static pmos_right_t fs_right;

const char *rootfs_path = "/";
const uint64_t rootfs_inode = 1;

static pmos_msgloop_tree_node_t fs_node;

enum EntryType {
    EntryDirectory,
    EntryFile, // Module
    Unknown,
};

int entry_type_to_ipc_type(enum EntryType type)
{
    switch (type) {
    case EntryDirectory:
        return IPC_FILE_TYPE_DIRECTORY;
    case EntryFile:
        return IPC_FILE_TYPE_REGULAR;
    default:
        return 0;
    }
}

unsigned entry_type_to_mode(enum EntryType type)
{
    switch (type) {
    case EntryDirectory:
        return S_IFDIR;
    case EntryFile:
        return S_IFREG;
    default:
        return 0;
    }
}

typedef struct {} inode_tree_node;
typedef struct {} path_tree_node; 

int inode_tree_compare(inode_tree_node *a, inode_tree_node *b);
int inode_tree_key_compare(inode_tree_node *a, uint64_t *b);

struct string_ref {
    const char *str;
    size_t len;
};

int path_tree_compare(path_tree_node *a, path_tree_node *b);
int path_tree_key_compare(path_tree_node *a, struct string_ref *b);

RBTREE(inode_tree, inode_tree_node, inode_tree_compare, inode_tree_key_compare);
RBTREE(path_tree, path_tree_node, path_tree_compare, path_tree_key_compare);

inode_tree_tree_t inode_tree = inode_tree_INITIALIZER;

struct tree_node {
    inode_tree_node_t inode_node;
    path_tree_node_t path_node;
    uint64_t inode;
    char *name;
    enum EntryType type;

    path_tree_tree_t children;
    struct module_descriptor_list *module; // Only for EntryFile
};

int inode_tree_compare(inode_tree_node *a, inode_tree_node *b)
{
    struct tree_node *node_a = (struct tree_node *)((char *)a - offsetof(struct tree_node, inode_node));
    struct tree_node *node_b = (struct tree_node *)((char *)b - offsetof(struct tree_node, inode_node));

    if (node_a->inode < node_b->inode)
        return -1;
    else if (node_a->inode > node_b->inode)
        return 1;
    else
        return 0;
}

int inode_tree_key_compare(inode_tree_node *a, uint64_t *b)
{
    struct tree_node *node_a = (struct tree_node *)((char *)a - offsetof(struct tree_node, inode_node));

    if (node_a->inode < *b)
        return -1;
    else if (node_a->inode > *b)
        return 1;
    else
        return 0;
}

int path_tree_compare(path_tree_node *a, path_tree_node *b)
{
    struct tree_node *node_a = (struct tree_node *)((char *)a - offsetof(struct tree_node, path_node));
    struct tree_node *node_b = (struct tree_node *)((char *)b - offsetof(struct tree_node, path_node));

    return strcmp(node_a->name, node_b->name);
}

int path_tree_key_compare(path_tree_node *a, struct string_ref *b)
{
    struct tree_node *node_a = (struct tree_node *)((char *)a - offsetof(struct tree_node, path_node));

    int cmp = strncmp(node_a->name, b->str, b->len);
    if (cmp == 0)
        return node_a->name[b->len];
    return cmp;
}

struct tree_node root_node = {
    .inode = 1,
    .name = "/",
    .type = EntryDirectory,
    .children = path_tree_INITIALIZER,
};
__attribute__((constructor)) void init_root_node()
{
    inode_tree_insert(&inode_tree, &root_node.inode_node);
}

struct tree_node *get_root()
{
    return &root_node;
}

struct tree_node *get_inode(uint64_t inode)
{
    inode_tree_node_t *node = inode_tree_find(&inode_tree, &inode);
    if (!node)
        return NULL;

    struct tree_node *tree_node = (struct tree_node *)((char *)node - offsetof(struct tree_node, inode_node));
    return tree_node;
}

uint64_t next_inode = 2; // 1 is reserved for root

bool path_is_valid(const char *path, size_t path_len)
{
    if (path_len == 0)
        return true; // Empty path is valid (root)

    const char *end = path + path_len;
    while (path < end) {
        const char *next_slash = memchr(path, '/', end - path);
        size_t segment_len = next_slash ? (size_t)(next_slash - path) : (size_t)(end - path);

        if (segment_len == 0) {
            // Skip empty segments (e.g., leading or consecutive slashes)
            path += 1;
            continue;
        }

        if (segment_len == 1 && path[0] == '.') {
            return false; // Current directory, not valid for this function
        }
        if (segment_len == 2 && path[0] == '.' && path[1] == '.') {
            return false; // Parent directory, not valid for this function
        }

        path += segment_len;
        if (next_slash)
            path += 1; // Skip the slash
    }
    return true;
}

struct tree_node *get_child(struct tree_node *parent, const char *name, size_t name_len)
{
    if (parent->type != EntryDirectory)
        return NULL;

    struct string_ref key = {name, name_len};
    path_tree_node_t *node = path_tree_find(&parent->children, &key);
    if (!node)
        return NULL;

    struct tree_node *child = (struct tree_node *)((char *)node - offsetof(struct tree_node, path_node));
    return child;
}

struct tree_node *get_directory(const char *path, size_t path_len)
{
    if (!path_is_valid(path, path_len))
        return NULL;

    struct tree_node *current = get_root();
    const char *end = path + path_len;
    while (path < end) {
        const char *next_slash = memchr(path, '/', end - path);
        size_t segment_len = next_slash ? (size_t)(next_slash - path) : (size_t)(end - path);

        if (segment_len == 0) {
            // Skip empty segments (e.g., leading or consecutive slashes)
            path += 1;
            continue;
        }

        struct tree_node *child = get_child(current, path, segment_len);
        if (child && child->type != EntryDirectory) {
            return NULL; // Not found or not a directory
        }

        if (!child) {
            // Directory does not exist, create it
            child = malloc(sizeof(struct tree_node));
            if (!child) {
                print_str("Loader: Could not allocate memory for directory node\n");
                return NULL;
            }

            child->inode = next_inode++;
            child->name = strndup(path, segment_len);
            child->type = EntryDirectory;
            child->children = path_tree_INITIALIZER;

            path_tree_insert(&current->children, &child->path_node);
            inode_tree_insert(&inode_tree, &child->inode_node);
        }

        current = child;
        path += segment_len;
        if (next_slash)
            path += 1; // Skip the slash
    }
    return current;
}

void fs_add_module(struct module_descriptor_list *module)
{
    if (!module)
        return;

    if (!module->path) {
        print_str("Loader: Module has no path, cannot add to filesystem\n");
        return;
    }

    struct tree_node *parent_dir = nullptr;
    size_t len = strlen(module->path);
    if (len == 0) {
        print_str("Loader: Module path is empty, cannot add to filesystem\n");
        return;
    }

    // Resolve directory
    const char *dir = strrchr(module->path, '/');
    const char *name = dir ? dir + 1 : module->path;

    if (strlen(name) == 0) {
        dbprintf("Loader: Module path '%s' has no filename, cannot add to filesystem\n", module->path);
        return;
    }
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        dbprintf("Loader: Module path '%s' has invalid filename '%s', cannot add to filesystem\n", module->path, name);
        return;
    }

    if (dir) {
        size_t dir_len = dir - module->path;
        parent_dir = get_directory(module->path, dir_len);
        if (!parent_dir) {
            dbprintf("Loader: Could not find parent directory for module '%s'\n", module->path);
            return;
        }
    } else {
        parent_dir = get_root();
    }

    if (parent_dir->type != EntryDirectory) {
        dbprintf("Loader: Parent path for module '%s' is not a directory\n", module->path);
        return;
    }

    if (get_child(parent_dir, name, strlen(name)) != NULL) {
        dbprintf("Loader: Module '%s' already exists in filesystem, cannot add\n", module->path);
        return;
    }

    struct tree_node *node = malloc(sizeof(struct tree_node));
    if (!node) {
        print_str("Loader: Could not allocate memory for filesystem node\n");
        return;
    }

    node->inode = next_inode++;
    node->name = strdup(name);
    node->type = EntryFile;
    node->children = path_tree_INITIALIZER;
    node->module = module;

    path_tree_insert(&parent_dir->children, &node->path_node);
    inode_tree_insert(&inode_tree, &node->inode_node);
}

void print_node(struct tree_node *node, int depth)
{
    dbprintf("%*s- %s (inode: %"PRIu64 ", type: %s)\n", depth * 2, "", node->name, node->inode,
             node->type == EntryDirectory ? "directory" : "file");
    
    path_tree_node_t *child_node = path_tree_first(&node->children);
    while (child_node) {
        struct tree_node *child = (struct tree_node *)((char *)child_node - offsetof(struct tree_node, path_node));
        print_node(child, depth + 1);
        child_node = path_tree_next(child_node);
    }
}

void print_fs_tree()
{
    dbprintf("Filesystem tree:\n");
    print_node(&root_node, 0);
}

void resolve_path_reply(int result, enum EntryType type, uint64_t inode, pmos_right_t *reply_right)
{
    IPC_FS_Resolve_Path_Reply reply = {
        .type = IPC_FS_Resolve_Path_Reply_NUM,
        .result_code = result,
        .file_type = entry_type_to_ipc_type(type),
        .file_id = inode,
        .st_mode = 0555 | entry_type_to_mode(type),
        .st_uid = 0,
        .st_gid = 0,
        .st_rdev = 0,
        .st_blksize = 4096,
    };

    auto r = send_message_right(*reply_right, 0, &reply, sizeof(reply), NULL, SEND_MESSAGE_DELETE_RIGHT);
    if (!r.result)
        *reply_right = 0;
    else
        dbprintf("Loader: Failed to send IPC_FS_Resolve_Path_Reply: %d\n", (int)r.result);
}

void resolve_path_msg(IPC_FS_Resolve_Path *msg, size_t msg_size, pmos_right_t *reply_right)
{
    if (msg_size < sizeof(IPC_FS_Resolve_Path)) {
        print_str("Loader: Received too small IPC_FS_Resolve_Path message\n");
        return;
    }

    const char *path = msg->path_name;
    size_t path_size = msg_size - sizeof(IPC_FS_Resolve_Path);
    size_t path_len = strnlen(path, path_size);
    if (path_len == 0) {
        print_str("Loader: Received invalid path in IPC_FS_Resolve_Path message\n");
        resolve_path_reply(-EINVAL, 0, 0, reply_right);
        return;
    }
    uint64_t inode = msg->inode;
    // dbprintf("Loader: Resolving path '%.*s' from inode %" PRIu64 "\n", (int)path_len, path, msg->inode);

    struct tree_node *parent_node = get_inode(inode);
    if (!parent_node) {
        resolve_path_reply(-ENOENT, 0, 0, reply_right);
        return;
    }

    struct tree_node *i = get_child(parent_node, path, path_len);
    if (!i) {
        resolve_path_reply(-ENOENT, 0, 0, reply_right);
        return;
    }

    resolve_path_reply(0, i->type, i->inode, reply_right);
}

struct OpenFileData {
    struct tree_node *node;
    uint64_t offset;
    pmos_msgloop_tree_node_t msgloop_node;
};

void read_reply(int result, pmos_right_t *reply_right)
{
    IPC_Read_Reply reply = {
        .type = IPC_Read_Reply_NUM,
        .result_code = result,
    };

    auto r = send_message_right(*reply_right, 0, &reply, sizeof(reply), NULL, SEND_MESSAGE_DELETE_RIGHT);
    if (!r.result)
        *reply_right = 0;
    else
        dbprintf("Loader: Failed to send IPC_Read_Reply: %d\n", (int)r.result);
}

int read_to_buffer(pmos_right_t mem_object, uint8_t *data, uint64_t start_offset, size_t size)
{
    void *ptr = NULL;
    int result = 0;

    uint64_t page_mask = getpagesize() - 1;
    uint64_t object_start = start_offset & ~page_mask;
    uint64_t start = start_offset & page_mask;
    uint64_t size_aligned = (start + size + page_mask) & ~page_mask;

    auto mem_request = map_mem_object(&(map_mem_object_param_t){
        .page_table_id = 0,
        .object_right = mem_object,
        .addr_start_uint = 0,
        .size = size_aligned,
        .offset_object = object_start,
        .offset_start = 0,
        .object_size = size_aligned,
        .access_flags = PROT_READ,
    });

    if (mem_request.result != SUCCESS) {
        dbprintf("Loader: Failed to map memory object %" PRIu64 " for read: %d\n", mem_object, (int)mem_request.result);
        result = -EIO;
        goto end;
    }

    ptr = mem_request.virt_addr;

    memcpy(data, (uint8_t *)ptr + start, size);

end:
    if (ptr)
        munmap(ptr, size_aligned);
    return result;
}

void handle_read(struct OpenFileData *open_file_data, IPC_Read *msg, pmos_right_t *reply_right)
{
    char *msg_buff = nullptr;
    if (open_file_data->node->type != EntryFile) {
        dbprintf("Loader: Attempted to read from non-file inode %" PRIu64 "\n", open_file_data->node->inode);
        read_reply(-EINVAL, reply_right);
        goto end;
    }

    bool fixed = msg->flags & IPC_FLAG_IO_OP_FIXED_OFFSET;
    uint64_t offset = fixed ? msg->start_offset : open_file_data->offset;
    uint64_t max_read = msg->max_size;

    auto module = open_file_data->node->module;
    if (module->size <= offset) {
        read_reply(0, reply_right);
        goto end;
    }

    size_t bytes_to_read = module->size - offset;
    bytes_to_read = bytes_to_read > max_read ? max_read : bytes_to_read;

    size_t msg_size = sizeof(IPC_Read_Reply) + bytes_to_read;

    msg_buff = malloc(msg_size);
    if (!msg_buff) {
        dbprintf("Loader: Failed to allocate buffer for read operation\n");
        read_reply(-ENOMEM, reply_right);
        goto end;
    }

    IPC_Read_Reply *reply = (IPC_Read_Reply *)msg_buff;
    reply->type = IPC_Read_Reply_NUM;
    reply->result_code = 0;
    reply->flags = 0;

    int result = read_to_buffer(module->object_right, reply->data, offset, bytes_to_read);
    if (result) {
        dbprintf("Loader: Failed to read from module object right: %d\n", result);
        read_reply(result, reply_right);
        goto end;
    }

    auto r = send_message_right(*reply_right, 0, reply, msg_size, NULL, SEND_MESSAGE_DELETE_RIGHT);
    if (!r.result) {
        if (!fixed)
            open_file_data->offset += bytes_to_read;
        *reply_right = 0;
    } else
        dbprintf("Loader: Failed to send IPC_Read_Reply: %d\n", (int)r.result);

end:
    free(msg_buff);
}

void handle_seek(struct OpenFileData *open_file_data, IPC_Seek *msg, pmos_right_t *reply_right)
{
    if (open_file_data->node->type != EntryFile) {
        dbprintf("Loader: Attempted to seek on non-file inode %" PRIu64 "\n", open_file_data->node->inode);
        read_reply(-EINVAL, reply_right);
        return;
    }

    uint64_t new_offset = 0;
    switch (msg->whence) {
    case SEEK_SET:
        new_offset = (uint64_t)msg->offset;
        break;
    case SEEK_CUR:
        new_offset = open_file_data->offset + msg->offset;
        break;
    case SEEK_END:
        new_offset = open_file_data->node->module->size + msg->offset;
        break;
    default:
        read_reply(-EINVAL, reply_right);
        return;
    }

    open_file_data->offset = new_offset;

    IPC_Seek_Reply reply = {
        .type = IPC_Seek_Reply_NUM,
        .result_code = 0,
        .new_offset = new_offset,
    };

    auto r = send_message_right(*reply_right, 0, &reply, sizeof(reply), NULL, SEND_MESSAGE_DELETE_RIGHT);
    if (!r.result)
        *reply_right = 0;
    else
        dbprintf("Loader: Failed to send IPC_Seek_Reply: %d\n", (int)r.result);
}

void ipc_open_error_reply(int result, pmos_right_t *reply_right)
{
    IPC_FS_Open_Reply reply = {
        .type = IPC_FS_Open_Reply_NUM,
        .result_code = result,
        .fs_flags = 0,
    };

    auto r = send_message_right(*reply_right, 0, &reply, sizeof(reply), NULL, SEND_MESSAGE_DELETE_RIGHT);
    if (!r.result)
        *reply_right = 0;
    else
        dbprintf("Loader: Failed to send IPC_FS_Open_Reply: %d\n", (int)r.result);
}

void get_object_reply(int result, pmos_right_t *reply_right)
{
    IPC_Get_Object_Reply reply = {
        .type = IPC_Get_Object_Reply_NUM,
        .result_code = result,
    };

    auto r = send_message_right(*reply_right, 0, &reply, sizeof(reply), NULL, SEND_MESSAGE_DELETE_RIGHT);
    if (!r.result)
        *reply_right = 0;
    else
        dbprintf("Loader: Failed to send IPC_Get_Object_Reply: %d\n", (int)r.result);
}

void handle_get_object(struct OpenFileData *open_file_data, IPC_Get_Object *msg, pmos_right_t *reply_right)
{
    if (open_file_data->node->type != EntryFile) {
        dbprintf("Loader: Attempted to get object from non-file inode %" PRIu64 "\n", open_file_data->node->inode);
        get_object_reply(-EINVAL, reply_right);
        return;
    }

    auto result = dup_right(open_file_data->node->module->object_right);
    if (result.result != SUCCESS) {
        dbprintf("Loader: Failed to duplicate object right for inode %" PRIu64 ": %d\n", open_file_data->node->inode, (int)result.result);
        get_object_reply(-EIO, reply_right);
        return;
    }

    auto restrict_result = restrict_right(result.right, RIGHT_PERMISSION_READ | RIGHT_PERMISSION_EXECUTE);
    if (restrict_result.result) {
        int result = (int)restrict_result.result;
        dbprintf("Loader: Failed to restrict the right, error %i (%s)", result, strerror(-result));
        get_object_reply(result, reply_right);
        return;
    }

    message_extra_t rights = {
        .extra_rights = {result.right}
    };
    IPC_Get_Object_Reply reply = {
        .type = IPC_Get_Object_Reply_NUM,
        .result_code = 0,
    };
    auto r = send_message_right(*reply_right, 0, &reply, sizeof(reply), &rights, SEND_MESSAGE_DELETE_RIGHT);
    if (!r.result)
        *reply_right = 0;
    else
        dbprintf("Loader: Failed to send IPC_Get_Object_Reply: %d\n", (int)r.result);
}

static int file_op_callback(Message_Descriptor *desc, void *buff, pmos_right_t *reply_right,
                              pmos_right_t *extra_rights, void *ctx, struct pmos_msgloop_data *data)
{
    (void)reply_right;
    (void)extra_rights;
    
    struct OpenFileData *open_file_data = ctx;

    if (desc->size < sizeof(IPC_Generic_Msg)) {
        print_str("Loader: Received very small message from filesystem\n");
        return -1;
    }

    IPC_Generic_Msg *ipc_msg = (IPC_Generic_Msg *)(buff);
    switch (ipc_msg->type) {
    case IPC_Kernel_Receive_Right_Destroyed_NUM:
        pmos_msgloop_erase(data, &open_file_data->msgloop_node);
        free(open_file_data);
        break;

    case IPC_Read_NUM: {
        IPC_Read *msg = (IPC_Read *)(buff);
        if (desc->size < sizeof(IPC_Read)) {
            dbprintf("Loader: Received IPC_Read of unexpected size 0x%x\n", (uint32_t)desc->size);
            break;
        }

        handle_read(open_file_data, msg, reply_right);
        break;
    }
    
    case IPC_Seek_NUM: {
        IPC_Seek *msg = (IPC_Seek *)(buff);
        if (desc->size < sizeof(IPC_Seek)) {
            dbprintf("Loader: Received IPC_Seek of unexpected size 0x%x\n", (uint32_t)desc->size);
            break;
        }

        handle_seek(open_file_data, msg, reply_right);
        break;
    }

    case IPC_Get_Object_NUM: {
        IPC_Get_Object *msg = (IPC_Get_Object *)(buff);
        if (desc->size < sizeof(IPC_Get_Object)) {
            dbprintf("Loader: Received IPC_Get_Object of unexpected size 0x%x\n", (uint32_t)desc->size);
            break;
        }

        handle_get_object(open_file_data, msg, reply_right);
        break;
    }

    default:
        dbprintf("Loader: Unknown message type 0x%x while attending file\n", ipc_msg->type);
        break;
    }

    return 0;
}

void ipc_fs_open(uint32_t flags, uint64_t inode, pmos_right_t *reply_right)
{
    struct OpenFileData *ofd = NULL;

    struct tree_node *node = get_inode(inode);
    if (!node) {
        ipc_open_error_reply(-ENOENT, reply_right);
        goto end;
    }

    ofd = malloc(sizeof(struct OpenFileData));
    if (!ofd) {
        dbprintf("Loader: Failed to allocate OpenFileData for inode %" PRIu64 "\n", inode);
        ipc_open_error_reply(-ENOMEM, reply_right);
        goto end;
    }

    pmos_right_t receive_right = INVALID_RIGHT;
    auto ret = create_right(loader_port, &receive_right, 0);
    if (ret.result != SUCCESS) {
        dbprintf("Loader: Failed to create receive right for open file: %i\n", (int)ret.result);
        ipc_open_error_reply(-ENOMEM, reply_right);
        goto end;
    }

    ofd->node = node;
    ofd->offset = 0;
    pmos_msgloop_node_set(&ofd->msgloop_node, receive_right, file_op_callback, ofd);

    IPC_FS_Open_Reply reply = {
        .type = IPC_FS_Open_Reply_NUM,
        .result_code = 0,
        .fs_flags = 0,
    };

    message_extra_t rights = {
        .extra_rights = {ret.right}
    };
    auto r = send_message_right(*reply_right, 0, &reply, sizeof(reply), &rights, SEND_MESSAGE_DELETE_RIGHT);
    if (r.result) {
        dbprintf("Loader: Failed to send IPC_FS_Open_Reply: %d\n", (int)r.result);
        delete_right(ret.right);
    } else {
        *reply_right = 0;
        pmos_msgloop_insert(&msgloop_data, &ofd->msgloop_node);
        ofd = NULL;
    }

end:
    free(ofd);
}

void ipc_fs_stat_dynamic(uint64_t inode, pmos_right_t *reply_right)
{
    struct tree_node *node = get_inode(inode);
    if (!node) {
        ipc_open_error_reply(-ENOENT, reply_right);
        return;
    }

    size_t page_size = getpagesize();
    uint64_t page_mask = page_size - 1;

    uint64_t size = node->module->size;
    uint64_t blocks = (size + page_mask) / page_size;

    IPC_FS_Stat_Dynamic_Reply reply = {
        .type = IPC_FS_Stat_Dynamic_Reply_NUM,
        .flags = 0,
        .result_code = 0,
        .st_size = size,
        .st_nlink = 1,
        .st_blocks = blocks,
    };

    auto r = send_message_right(*reply_right, 0, &reply, sizeof(reply), NULL, SEND_MESSAGE_DELETE_RIGHT);
    if (!r.result)
        *reply_right = 0;
    else
        dbprintf("Loader: Failed to send IPC_FS_Stat_Dynamic_Reply: %d\n", (int)r.result);
}

static int filesystem_callback(Message_Descriptor *desc, void *buff, pmos_right_t *reply_right,
                              pmos_right_t *extra_rights, void *ctx, struct pmos_msgloop_data *data)
{
    (void)extra_rights;
    (void)ctx;

    if (desc->size < sizeof(IPC_Generic_Msg)) {
        print_str("Loader: Received very small message from filesystem\n");
        return -1;
    }

    IPC_Generic_Msg *ipc_msg = (IPC_Generic_Msg *)(buff);
    switch (ipc_msg->type) {
    case IPC_Kernel_Receive_Right_Destroyed_NUM:
        print_str("Loader: Received IPC_Kernel_Receive_Right_Destroyed from filesystem\n");
        pmos_msgloop_erase(data, &fs_node);
        // TODO: This is a good place to re-mount as root (if the posix server restarts)
        break;
    case IPC_FS_Resolve_Path_NUM: {
        IPC_FS_Resolve_Path *msg = (IPC_FS_Resolve_Path *)(buff);
        resolve_path_msg(msg, desc->size, reply_right);
        break;
    }

    case IPC_FS_Open_NUM: {
        IPC_FS_Open *msg = (IPC_FS_Open *)(buff);
        if (desc->size < sizeof(IPC_FS_Open)) {
            dbprintf("Loader: Received IPC_FS_Open of unexpected size 0x%x\n", (uint32_t)desc->size);
            break;
        }

        ipc_fs_open(msg->flags, msg->inode, reply_right);

        break;
    }

    case IPC_FS_Stat_Dynamic_NUM: {
        IPC_FS_Stat_Dynamic *msg = (IPC_FS_Stat_Dynamic *)(buff);
        if (desc->size < sizeof(IPC_FS_Stat_Dynamic)) {
            dbprintf("Loader: Received IPC_FS_Stat_Dynamic of unexpected size 0x%x\n", (uint32_t)desc->size);
            break;
        }

        ipc_fs_stat_dynamic(msg->inode, reply_right);
        break;
    }

    default:
        dbprintf("Loader: Unknown message type 0x%x from filesystem\n", ipc_msg->type);
        break;
    }

    return 0;
}

void mount_as_root()
{
    pmos_right_t vfs_right = INVALID_RIGHT;
    void *message = nullptr;

    if (posix_server_right == INVALID_RIGHT) {
        print_str("Loader: posix server right is invalid, cannot mount as root\n");
        goto error;
    }

    auto ret = create_right(loader_port, &fs_right, 0);
    if (ret.result != SUCCESS) {
        print_str("Loader: failed to create filesystem right: ");
        print_hex(ret.result);
        print_str("\n");
        goto error;
    }
    vfs_right = ret.right;

    pmos_msgloop_node_set(&fs_node, fs_right, filesystem_callback, NULL);
    pmos_msgloop_insert(&msgloop_data, &fs_node);

    size_t str_len = strlen(rootfs_path);
    size_t struct_len = sizeof(IPC_Mount_FS) + str_len;
    IPC_Mount_FS *msg = alloca(struct_len);

    msg->num = IPC_Mount_FS_NUM;
    msg->flags = 0;
    msg->root_fd = rootfs_inode;
    memcpy(msg->mount_path, rootfs_path, str_len);

    message_extra_t rights = {
        .extra_rights = {vfs_right}
    };

    right_request_t reply_right = send_message_right(posix_server_right, request_port, msg, struct_len, &rights, 0);
    if (reply_right.result != SUCCESS) {
        print_str("Loader: failed to send mount request to posix server: ");
        print_hex(reply_right.result);
        print_str("\n");
        goto error;
    }
    vfs_right = INVALID_RIGHT; // The right has been sent away

    Message_Descriptor desc;
    result_t msg_result = get_message(&desc, (unsigned char **)&message, request_port, NULL, NULL);
    if (msg_result) {
        print_str("Loader: Failed to get reply message from posix server: ");
        print_hex(msg_result);
        print_str("\n");
        goto error;
    }

    if (desc.size < sizeof(IPC_Generic_Msg)) {
        print_str("Loader: Received very small message from posix server\n");
        goto error;
    }

    IPC_Generic_Msg *reply_msg = (IPC_Generic_Msg *)(message);
    if (reply_msg->type != IPC_Mount_FS_Reply_NUM) {
        print_str("Loader: Received unexpected message type from posix server: ");
        print_hex(reply_msg->type);
        print_str("\n");
        goto error;
    }

    IPC_Mount_FS_Reply *reply = (IPC_Mount_FS_Reply *)(message);
    if (reply->result_code != 0) {
        print_str("Loader: Received error code from posix server: ");
        print_hex(reply->result_code);
        print_str("\n");
        goto error;
    }

    print_str("Loader: Mounted filesystem as root successfully\n");
error:
    if (message)
        free(message);
    if (vfs_right != INVALID_RIGHT) {
        delete_right(vfs_right);
    }
    return;
}