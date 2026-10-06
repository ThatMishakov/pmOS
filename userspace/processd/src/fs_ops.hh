#pragma once

#include <pmos/helpers.hh>

pmos::async::task<std::expected<void, int>> read_file(pmos::Right &file_right, std::span<uint8_t> buffer, size_t offset);