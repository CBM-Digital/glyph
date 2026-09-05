#include "profile/ProfileStore.h"
#include "script/Lexer.h"
#include "script/Parser.h"
#include "script/Error.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace glyph::profile {
namespace {
using namespace script;
Value literal(const AstPtr& node, unsigned depth = 0) {
  if (depth > 64) throw RuntimeError("profile nesting exceeds limit");
  switch (node->kind) {
    case AstKind::Nil: return Value::nil();
    case AstKind::Bool: return Value::booleanValue(node->boolean);
    case AstKind::Number:
      if (!std::isfinite(node->number)) throw RuntimeError("non-finite profile number");
      return Value::numberValue(node->number);
    case AstKind::String: return Value::stringValue(node->text);
    case AstKind::Keyword: return Value::keywordValue(node->id);
    case AstKind::Vector: {
      std::vector<Value> items;
      for (const auto& child : node->children) items.push_back(literal(child, depth + 1));
      return Value::vectorValue(std::move(items));
    }
    case AstKind::Map: {
      std::map<StringId, Value> items;
      for (const auto& [key, value] : node->entries) {
        if (key->kind != AstKind::Keyword) throw RuntimeError("profile keys must be keywords");
        items[key->id] = literal(value, depth + 1);
      }
      return Value::mapValue(std::move(items));
    }
    default: throw RuntimeError("profile contains executable code");
  }
}
std::string encodeValue(const Value& value, const StringInterner& names, unsigned depth) {
  if (depth > 64) throw RuntimeError("profile nesting exceeds limit");
  std::ostringstream out;
  switch (value.kind) {
    case ValueKind::Nil: return "nil";
    case ValueKind::Bool: return value.boolean ? "true" : "false";
    case ValueKind::Number:
      if (!std::isfinite(value.number)) throw RuntimeError("non-finite profile number");
      out << std::setprecision(std::numeric_limits<double>::max_digits10) << value.number;
      return out.str();
    case ValueKind::String:
      out << '"';
      for (char c : value.text) {
        switch(c) {
          case '\\': out << "\\\\"; break;
          case '"': out << "\\\""; break;
          case '\n': out << "\\n"; break;
          case '\t': out << "\\t"; break;
          case '\r': out << "\\r"; break;
          default: out << c;
        }
      }
      out << '"'; return out.str();
    case ValueKind::Keyword: return std::string(names.resolve(value.id));
    case ValueKind::Vector:
      out << '[';
      for (const auto& v : *value.vector) out << encodeValue(v,names,depth+1) << ' ';
      out << ']'; return out.str();
    case ValueKind::Map:
      out << '{';
      for (const auto& [key,v] : *value.map) out << names.resolve(key) << ' ' << encodeValue(v,names,depth+1) << ' ';
      out << '}'; return out.str();
    default: throw RuntimeError("profile values must be plain data");
  }
}
bool replaceFile(const std::filesystem::path& from, const std::filesystem::path& to) {
#ifdef _WIN32
  return MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  std::error_code ec;
  std::filesystem::rename(from,to,ec);
  return !ec;
#endif
}
}
std::string encode(const script::Value& v, const StringInterner& n) { return encodeValue(v,n,0); }
script::Value decode(const std::string& text, StringInterner& names) {
  if (text.size() > 4 * 1024 * 1024) throw script::RuntimeError("profile exceeds size limit");
  script::Lexer lexer(text);
  script::Parser parser(lexer.lex(), names, "<profile>");
  const auto program = parser.parseProgram();
  if (program.size()!=1) throw script::RuntimeError("profile must contain one data value");
  return literal(program.front());
}
ProfileStore::ProfileStore(std::filesystem::path path) : path_(std::move(path)) {
  if (path_.empty()) return;
  if (!std::filesystem::exists(path_) && !std::filesystem::exists(path_.string()+".bak")) return;
  if (!load(path_)) {
    if (!writable_) return; // Never downgrade a profile created by a newer release.
    if (load(path_.string()+".bak")) error_ = "Recovered progress from backup";
    else { entries_.clear(); error_ = "Could not read progress; original file retained"; writable_ = false; }
  }
}
bool ProfileStore::load(const std::filesystem::path& path) {
  try {
    std::ifstream input(path);
    if (!input || std::filesystem::file_size(path)>4*1024*1024) return false;
    std::ostringstream text; text << input.rdbuf();
    StringInterner names;
    const auto root = decode(text.str(), names);
    if (root.kind!=script::ValueKind::Map) return false;
    auto version = root.map->find(names.intern(":version"));
    if (version==root.map->end() || version->second.kind!=script::ValueKind::Number) return false;
    if (version->second.number>1) { writable_=false; error_="Progress belongs to a newer release"; return false; }
    if (version->second.number!=1) return false;
    const auto data = root.map->find(names.intern(":data"));
    if (data==root.map->end() || data->second.kind!=script::ValueKind::Map) return false;
    std::map<std::string,std::string> next;
    for(const auto& [key,value] : *data->second.map) next[std::string(names.resolve(key))]=encode(value,names);
    entries_=std::move(next); return true;
  } catch(const std::exception&) { return false; }
}
script::Value ProfileStore::get(const std::string& key, const script::Value& fallback, StringInterner& names) const {
  auto found=entries_.find(key);
  return found==entries_.end() ? fallback : decode(found->second,names);
}
bool ProfileStore::set(const std::string& key,const script::Value& value,const StringInterner& names) {
  if (key.empty() || key.front()!=':' || key.size()>128) throw script::RuntimeError("profile key must be a keyword");
  const auto data=encode(value,names);
  if (data.size()>1024*1024) throw script::RuntimeError("profile value exceeds size limit");
  if (entries_[key]==data) return writable_;
  entries_[key]=data;
  return flush();
}
bool ProfileStore::flush() {
  if (!writable_) return false;
  if (path_.empty()) return true;
  try {
    if (!path_.parent_path().empty()) std::filesystem::create_directories(path_.parent_path());
    std::ostringstream contents;
    contents << "{:version 1 :data {";
    for(const auto& [key,value] : entries_) contents << key << ' ' << value << '\n';
    contents << "}}\n";
    const auto write=[&](const std::filesystem::path& target){
      std::ofstream stream(target,std::ios::binary|std::ios::trunc);
      stream << contents.str(); stream.flush();
      return stream.good();
    };
    const auto temp=path_.string()+".tmp";
    if (!write(temp) || !replaceFile(temp,path_)) throw std::runtime_error("atomic replacement failed");
    const auto backup=path_.string()+".bak";
    if (!write(backup+".tmp") || !replaceFile(backup+".tmp",backup)) throw std::runtime_error("backup failed");
    error_.clear(); return true;
  } catch(const std::exception&) { error_="Progress could not be saved"; return false; }
}
bool ProfileStore::complete(const script::Value& result,StringInterner& names) {
  using namespace script;
  if(result.kind!=ValueKind::Map) throw RuntimeError("arcade/complete requires a result map");
  const auto field=[&](const Value& v,const char* k,Value fallback){ auto it=v.map->find(names.intern(k)); return it==v.map->end()?fallback:it->second; };
  const auto game=field(result,":game",Value::nil());
  const auto score=field(result,":score",Value::numberValue(0));
  const auto medals=field(result,":medals",Value::numberValue(0));
  if(game.kind!=ValueKind::Keyword || score.kind!=ValueKind::Number || medals.kind!=ValueKind::Number || !std::isfinite(score.number) || !std::isfinite(medals.number)) throw RuntimeError("invalid arcade result");
  const std::string key=std::string(names.resolve(game.id))+"-record";
  auto previous=get(key,Value::mapValue({}),names);
  if(previous.kind!=ValueKind::Map) previous=Value::mapValue({});
  const auto best=field(previous,":score",Value::numberValue(0));
  const auto medal=field(previous,":medals",Value::numberValue(0));
  auto record=*result.map;
  record[names.intern(":score")]=Value::numberValue(std::max(score.number,best.kind==ValueKind::Number?best.number:0));
  record[names.intern(":medals")]=Value::numberValue(std::max(medals.number,medal.kind==ValueKind::Number?medal.number:0));
  return set(key,Value::mapValue(std::move(record)),names);
}
}
