#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace glyph::assets {

class IAssetSource {
public:
  virtual ~IAssetSource() = default;

  virtual std::optional<std::string> readText(std::string_view path) const;
  virtual std::optional<std::vector<std::uint8_t>> readBytes(std::string_view path) const = 0;
  virtual bool exists(std::string_view path) const = 0;
  virtual std::optional<std::filesystem::file_time_type> lastWriteTime(std::string_view path) const;
  virtual std::optional<std::filesystem::path> physicalPath(std::string_view path) const;
};

class FilesystemAssetSource final : public IAssetSource {
public:
  explicit FilesystemAssetSource(std::filesystem::path root);

  std::optional<std::vector<std::uint8_t>> readBytes(std::string_view path) const override;
  bool exists(std::string_view path) const override;
  std::optional<std::filesystem::file_time_type> lastWriteTime(std::string_view path) const override;
  std::optional<std::filesystem::path> physicalPath(std::string_view path) const override;

  const std::filesystem::path& root() const;

private:
  std::optional<std::filesystem::path> resolve(std::string_view path) const;

  std::filesystem::path root_;
};

} // namespace glyph::assets
