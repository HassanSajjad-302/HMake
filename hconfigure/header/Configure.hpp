#ifndef HMAKE_CONFIGURE_HPP
#define HMAKE_CONFIGURE_HPP
#ifdef USE_HEADER_UNITS
#include "BuildSystemFunctions.hpp"
#include "BuildTools.hpp"
#include "Builder.hpp"
#include "Cache.hpp"
#include "Environment.hpp"
#include "Features.hpp"
#include "FileAndDirectory.hpp"
#include "JConsts.hpp"
#include "Settings.hpp"
#include "Targets.hpp"
#include "TarjanNode.hpp"
#include "fmt/color.h"
#include "nlohmann/json.hpp"
#include <filesystem>
#include <memory>
#include <set>
#include <stack>
#include <thread>
#include <utility>
#else
#include "BasicTargets.hpp"
#include "BuildSystemFunctions.hpp"
#include "BuildTools.hpp"
#include "Builder.hpp"
#include "Cache.hpp"
#include "CppSourceTarget.hpp"
#include "Features.hpp"
#include "GetTarget.hpp"
#include "JConsts.hpp"
#include "SMFile.hpp"
#include "Settings.hpp"
#include "TarjanNode.hpp"
#include "ToolsCache.hpp"
#include "Utilities.hpp"
#include "Variant.hpp"
#include "fmt/color.h"
#include "nlohmann/json.hpp"
#include <filesystem>
#include <memory>
#include <set>
#include <stack>
#include <thread>
#include <utility>
#endif
#endif // HMAKE_CONFIGURE_HPP
