#include "devfs.hh"
#include "vfs.hh"
#include <memory>
#include "log.hh"

struct DevVfs final: public Filesystem {
    std::expected<std::shared_ptr<VNode>, int> resolve_child(std::shared_ptr<VNode> parent, const std::string &name) override
    {
        (void)parent;
        (void)name;

        return std::unexpected(-ENOENT);
    }

    pmos::async::task<std::expected<pmos::Right, int>> open_file(std::shared_ptr<VNode> vnode, std::shared_ptr<Process>) override
    {
        kernelLogger() << "posixd: Attempted to open a file on the devfs\n" << frg::endlog;
        co_return std::unexpected(-ENOSYS); // TODO
    }

    pmos::async::task<std::expected<StatData, int>> get_file_stat_dynamic(std::shared_ptr<VNode> vnode)
    {
        kernelLogger() << "posixd: Attempted to get file stat on the devfs\n" << frg::endlog;
        co_return std::unexpected(-ENOSYS); // TODO
    }
};

std::shared_ptr<Filesystem> dev_fs;
std::shared_ptr<VNode> devfs_root_vnode;

void init_devfs()
{
    dev_fs = std::make_shared<DevVfs>();

    devfs_root_vnode = std::make_shared<VNode>();
    devfs_root_vnode->type = FileType::Directory;
    devfs_root_vnode->parent_fs = dev_fs;
    dev_fs->root = devfs_root_vnode;
    devfs_root_vnode->name = "dev";
}