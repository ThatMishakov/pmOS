#pragma once
#include <memory>
#include "vfs.hh"

void init_devfs();

extern std::shared_ptr<Filesystem> dev_fs;
extern std::shared_ptr<VNode> devfs_root_vnode;