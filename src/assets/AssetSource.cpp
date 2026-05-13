#include "assets/AssetSource.h"

#include <fstream>
#include <iterator>
#include <system_error>

namespace glyph::assets {
namespace {

std::filesystem::path canonicalRoot(const std::filesystem::path& root) {
  std::error_code ec;
  auto canonical = std::filesystem::weakly_canonical(root, ec);
  return ec ? std::filesystem::absolute(root) : canonical;
}

bool pathInsideRoot(const std::filesystem::path& root, const std::filesystem::path& candidate) {
  const auto relative = candidate.lexically_relative(root);
  if (relative.empty()) {
    return true;
  }

  auto it = relative.begin();
  return it != relative.end() && *it != ".." && !relative.is_absolute();
}

} // namespace

std::optional<std::string> IAssetSource::readText(std::string_view path) const {
  auto bytes = readBytes(path);
  if (!bytes) {
    return std::nullopt;
  }
  std::string text;
  text.reserve(bytes->size());
  for (std::uint8_t byte : *bytes) {
    text.push_back(static_cast<char>(byte));
  }
  return text;
}

std::optional<std::filesystem::file_time_type> IAssetSource::lastWriteTime(std::string_view) const {
  return std::nullopt;
}

std::optional<std::filesystem::path> IAssetSource::physicalPath(std::string_view) const {
  return std::nullopt;
}

FilesystemAssetSource::FilesystemAssetSource(std::filesystem::path root) : root_(canonicalRoot(root)) {}

std::optional<std::vector<std::uint8_t>> FilesystemAssetSource::readBytes(std::string_view path) const {
  const auto resolved = resolve(path);
  if (!resolved) {
    return std::nullopt;
  }

  std::ifstream input(*resolved, std::ios::binary);
  if (!input) {
    return std::nullopt;
  }
  return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool FilesystemAssetSource::exists(std::string_view path) const {
  const auto resolved = resolve(path);
  return resolved.has_value() && std::filesystem::exists(*resolved);
}

std::optional<std::filesystem::file_time_type> FilesystemAssetSource::lastWriteTime(std::string_view path) const {
  const auto resolved = resolve(path);
  if (!resolved) {
    return std::nullopt;
  }

  std::error_code ec;
  const auto writeTime = std::filesystem::last_write_time(*resolved, ec);
  if (ec) {
    return std::nullopt;
  }
  return writeTime;
}

std::optional<std::filesystem::path> FilesystemAssetSource::physicalPath(std::string_view path) const {
  return resolve(path);
}

const std::filesystem::path& FilesystemAssetSource::root() const { return root_; }

std::optional<std::filesystem::path> FilesystemAssetSource::resolve(std::string_view path) const {
  const std::filesystem::path raw{std::string(path)};
  if (raw.empty() || raw.is_absolute()) {
    return std::nullopt;
  }

  std::error_code ec;
  auto candidate = std::filesystem::weakly_canonical(root_ / raw, ec);
  if (ec) {
    candidate = std::filesystem::absolute(root_ / raw).lexically_normal();
  }
  if (!pathInsideRoot(root_, candidate)) {
    return std::nullopt;
  }
  return candidate;
}

} // namespace glyph::assets
