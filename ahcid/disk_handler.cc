#include "disk_handler.hh"

#include <unordered_map>
#include <memory>

using dptr = std::unique_ptr<DiskHandler>;
std::unordered_map<uint64_t, dptr> handlers;
uint64_t id = 1;
auto new_id() {
    return id++;
}

DiskHandler *DiskHandler::create(int port, uint64_t sector_count, std::size_t logical_sector_size, std::size_t physical_sector_size) {
    auto d = dptr(new DiskHandler);
    d->disk_id = new_id();
    d->sector_count = sector_count;
    d->logical_sector_size = logical_sector_size;
    d->physical_sector_size = physical_sector_size;
    d->port = port;

    auto p = d.get();

    handlers.insert({d->disk_id, std::move(d)});
    return p;
}

void DiskHandler::destroy() noexcept {
    handlers.erase(disk_id);
}

DiskHandler *DiskHandler::get(uint64_t id) {
    return handlers.at(id).get();
}