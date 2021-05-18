
#ifndef HMAKE_PROJECT_HPP
#define HMAKE_PROJECT_HPP

#include "string"
#include "executable.hpp"
#include "library.hpp"
#include "configurationType.hpp"
#include "nlohmann/json.hpp"

struct projectVersion{
    int majorVersion{};
    int minorVersion{};
    int patchVersion{};
};
struct project {
public:
    project(std::string projectName, projectVersion version = projectVersion());
    static std::vector<library> projectLibraries;
    static std::vector<executable> projectExecutables;
    static inline std::string PROJECT_NAME;
    static inline projectVersion PROJECT_VERSION;
    static inline fs::path CXX_COMPILER;
    static inline directory SOURCE_DIRECTORY;
    static inline directory BUILD_DIRECTORY;
    static inline configurationType projectConfigurationType;
};
typedef nlohmann::json json;
void to_json(json& j, const projectVersion& p);
void to_json(json&j, const configurationType& p);
void to_json(json& j, const project& p);

#endif //HMAKE_PROJECT_HPP
