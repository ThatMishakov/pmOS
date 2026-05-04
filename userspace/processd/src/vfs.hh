#pragma once
#include <vector>
#include <string>
#include <pmos/async/coroutines.hh>

struct VNode;

struct Filesystem {
    pmos::Right fs_right;
    std::string mountpoint;

    std::shared_ptr<VNode> root;
};

enum class FileType {
    None,
    File,
    Directory,
    // TODO
};

struct VNode {
    std::shared_ptr<Filesystem> parent_fs;
    std::weak_ptr<VNode> parent;
    int64_t inode = 0;
    std::string name;

    bool is_dir = false;
    FileType type = FileType::None;

    std::map<std::string, std::shared_ptr<VNode>> children;
};

extern std::vector<std::shared_ptr<Filesystem>> filesystems;
extern std::shared_ptr<VNode> root_vnode;


struct Path {
    static Path parse(const std::string &path);

    inline bool relative() const
    {
        return _relative;
    }
    inline bool trailing_slash() const
    {
        return _trailing_slash;
    }
    inline const std::vector<std::string> &components() const
    {
        return _components;
    }


    bool _relative = false;
    bool _trailing_slash = false;
    std::vector<std::string> _components;
};