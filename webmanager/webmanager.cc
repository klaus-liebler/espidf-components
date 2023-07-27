#include "webmanager.hh"
#include <array>
webmanager::M* webmanager::M::singleton{nullptr};
std::array<webmanager::MessageLogEntry, webmanager::STORAGE_LENGTH> webmanager::M::BUFFER;
