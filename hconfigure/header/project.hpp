
#ifndef HMAKE_PROJECT_HPP
#define HMAKE_PROJECT_HPP

#include "string"
#include "executable.hpp"
#include "library.hpp"
#include "CONFIG_TYPE.hpp"
#include "nlohmann/json.hpp"
#include "flags.hpp"
#include "set"
struct projectVersion{
    int majorVersion{};
    int minorVersion{};
    int patchVersion{};
};
class project {
private:
    static CONFIG_TYPE currentConfigurationType;
    static inline bool mainProjectFile = true;
public:
    project(std::string projectName, projectVersion version = projectVersion());
    static inline std::string PROJECT_NAME;
    static inline projectVersion PROJECT_VERSION;
    static inline directory SOURCE_DIRECTORY;
    static inline directory BUILD_DIRECTORY;
    static inline CONFIG_TYPE projectConfigurationType;
    static inline compiler ourCompiler;
    static inline linker ourLinker;
    static inline std::vector<executable> projectExecutables;
    static inline std::vector<library> projectLibraries;
    static inline Flags flags;
};

void to_json(json& j, const projectVersion& p);
void to_json(json&j, const CONFIG_TYPE& p);
void to_json(json& j, const project& p);

#endif //HMAKE_PROJECT_HPP
