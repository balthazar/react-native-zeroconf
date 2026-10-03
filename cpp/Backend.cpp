#include "Backend.h"

namespace rnzeroconf {

Error LibraryError(const std::string &code, const std::string &message, const std::string &serviceName) {
  return Error{"RNZeroconf", code, message, serviceName};
}

std::string SubtypeLabel(const std::string &subtype) {
  if (subtype.empty() || subtype[0] == '_') {
    return subtype;
  }
  return "_" + subtype;
}

} // namespace rnzeroconf
