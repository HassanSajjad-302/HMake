#include "Variant.hpp"
#include "BuildSystemFunctions.hpp"
#include "JConsts.hpp"
#include "Target.hpp"
#include <memory>

using std::make_shared;

bool Variant::TargetComparator::operator()(const class Target *lhs, const class Target *rhs) const
{
    return lhs->targetName < rhs->targetName;
}

Variant::Variant(const string &name_, const bool initializeFromCache) : CTarget{name_}
{
    if (initializeFromCache)
    {
        initializeFromCacheFunc();
    }
    cTargetType = TargetType::VARIANT;
}

// TODO: Will Not be copying prebuilts and packagedLibs for Now.
void Variant::copyAllTargetsFromOtherVariant(Variant &variantFrom)
{
    for (CTarget *element : variantFrom.elements)
    {
        if (element->cTargetType == TargetType::EXECUTABLE)
        {
            auto *exe = static_cast<Executable *>(element);
            targetsContainer.emplace_back(make_shared<Executable>(*exe));
        }
        // TODO
        //  Not doing for libraries
    }
}

void Variant::setJsonDerived()
{
    set<TarjanNode<Dependent>> tarjanNodesLibraryPointers;
    {
        // TODO
        // This does not work because prebuilts is not public. change this in a way that it does not effect
        // PPLibrary which is derived from PLibrary
        /*        for (PLibrary *pLibrary : preBuiltLibraries)
                {
                    auto [pos, Ok] = tarjanNodesLibraryPointers.emplace(pLibrary);
                    for(PLibrary *pLibrary1 : pLibrary->prebuilts)
                    {
                        auto [posDep, Ok1] = tarjanNodesLibraryPointers.emplace(pLibrary1);
                        pos->deps.emplace_back(const_cast<TarjanNode<Dependent> *>(&(*posDep)));
                    }
                }*/

        // TODO
        // Packaged Libs currently can't have dependencies. Need to revise Packaging and this decision. However,
        // currently allowing prebuilts to have other prebuilts as dependencies so to help package a project that
        // was not compiled in HMake.
        auto populateTarjanNode = [&tarjanNodesLibraryPointers](const auto &targets) {
            for (CTarget *tar : targets)
            {
                auto *target = static_cast<Target *>(tar);
                auto [pos, Ok] = tarjanNodesLibraryPointers.emplace(target);
                auto b = pos;

                auto populateDepsOfTarjanNode = [&b, &tarjanNodesLibraryPointers](auto &dependentVector) {
                    for (Dependent *dependent : dependentVector)
                    {
                        auto [posDep, Ok1] = tarjanNodesLibraryPointers.emplace(dependent);
                        b->deps.emplace(const_cast<TarjanNode<Dependent> *>(&(*posDep)));
                    }
                };

                populateDepsOfTarjanNode(target->publicLibs);
                populateDepsOfTarjanNode(target->privateLibs);
                populateDepsOfTarjanNode(target->publicPrebuilts);
                populateDepsOfTarjanNode(target->privatePrebuilts);
            }
        };

        populateTarjanNode(elements);

        TarjanNode<Dependent>::tarjanNodes = &tarjanNodesLibraryPointers;
        TarjanNode<Dependent>::findSCCS();
        // TODO
        // This gives error which should not be the case.
        TarjanNode<Dependent>::checkForCycle([](Dependent *dependent) -> string {
            // return getStringFromVariant(dependent);
            return "dsd";
        });
        vector<Dependent *> topologicallySorted = TarjanNode<Dependent>::topologicalSort;

        // TODO
        // Not confirm on the order of the topologicallySorted. Assuming that next ones are dependencies of previous
        // ones
        for (auto it = topologicallySorted.begin(); it != topologicallySorted.end(); ++it)
        {
            Dependent *dependent = it.operator*();
            if (dependent->isTarget)
            {
                auto *target = static_cast<Target *>(dependent);
                for (Library *library : target->publicLibs)
                {
                    target->publicLibs.insert(library->publicLibs.begin(), library->publicLibs.end());
                    target->publicPrebuilts.insert(library->publicPrebuilts.begin(), library->publicPrebuilts.end());
                }

                // TODO: Following is added to compile the examples. However, the code should also add other public
                // properties from public dependencies
                // Modify PLibrary to be dependent of Dependency, thus having public properties.
                for (Library *lib : target->publicLibs)
                {
                    for (const Node *idd : lib->publicIncludes)
                    {
                        target->publicIncludes.emplace_back(idd);
                    }
                }
                for (Library *lib : target->publicLibs)
                {
                    for (const Node *idd : lib->publicHUIncludes)
                    {
                        target->publicIncludes.emplace_back(idd);
                    }
                }
            }
        }
    }

    Json variantJson;
    variantJson[JConsts::configuration] = configurationType;
    variantJson[JConsts::compiler] = compiler;
    variantJson[JConsts::linker] = linker;
    variantJson[JConsts::archiver] = archiver;
    variantJson[JConsts::compilerFlags] = "";
    variantJson[JConsts::linkerFlags] = "";
    variantJson[JConsts::libraryType] = libraryType;
    variantJson[JConsts::environment] = environment;
    variantJson[JConsts::targets] = elements;
    variantJson[JConsts::targetsWithModules] = moduleTargets;
    json = variantJson;
}

Executable &Variant::findExecutable(const string &exeName)
{
    CTarget exe(exeName);
    auto it = elements.find(&exe);
    if (it == elements.end() || it.operator*()->cTargetType != TargetType::EXECUTABLE)
    {
        print(stderr, "Could Not find the Executable {} in Variant {}\n", exeName, name);
        exit(EXIT_FAILURE);
    }
    return static_cast<Executable &>(**it);
}

Library &Variant::findLibrary(const string &libName)
{
    CTarget lib(libName);
    auto it = elements.find(&lib);
    if (it == elements.end() || (it.operator*()->cTargetType != TargetType::LIBRARY_STATIC ||
                                 it.operator*()->cTargetType != TargetType::LIBRARY_SHARED))
    {
        print(stderr, "Could Not find the Library {} in Variant {}\n", libName, name);
        exit(EXIT_FAILURE);
    }
    return static_cast<Library &>(**it);
}

Executable &Variant::addExecutable(const string &exeName)
{
    return static_cast<Executable &>(
        targetsContainer.emplace_back(make_shared<Executable>(exeName, *this)).operator*());
}

Library &Variant::addLibrary(const string &libName)
{
    return static_cast<Library &>(targetsContainer.emplace_back(make_shared<Library>(libName, *this)).operator*());
}
