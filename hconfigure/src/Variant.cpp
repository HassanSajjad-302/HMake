#include "Variant.hpp"
#include "BuildSystemFunctions.hpp"
#include "CppSourceTarget.hpp"
#include "JConsts.hpp"
#include <memory>

using std::make_shared;

bool Variant::TargetComparator::operator()(const class CppSourceTarget *lhs, const class CppSourceTarget *rhs) const
{
    return lhs->name < rhs->name;
}

Variant::Variant(const string &name_, const bool initializeFromCache) : CTarget{name_}
{
    if (initializeFromCache)
    {
        CompilerFeatures::initializeFromCacheFunc();
        LinkerFeatures::initializeFromCacheFunc();
    }
}

// TODO: Will Not be copying prebuilts and packagedLibs for Now.
void Variant::copyAllTargetsFromOtherVariant(Variant &variantFrom)
{
    /*    for (CTarget *element : variantFrom.elements)
        {
            if (element->cTargetType == TargetType::EXECUTABLE)
            {
                auto *exe = static_cast<Executable *>(element);
                targetsContainer.emplace_back(make_shared<Executable>(*exe));
            }
            // TODO
            //  Not doing for libraries
        }*/
}

void Variant::setJson()
{
    Json variantJson;
    variantJson[JConsts::configuration] = configurationType;
    variantJson[JConsts::compiler] = compiler;
    variantJson[JConsts::linker] = linker;
    variantJson[JConsts::archiver] = archiver;
    variantJson[JConsts::compilerFlags] = "";
    variantJson[JConsts::linkerFlags] = "";
    variantJson[JConsts::libraryType] = libraryType;
    variantJson[JConsts::targets] = elements;
    variantJson[JConsts::targetsWithModules] = moduleTargets;
    json = variantJson;
}
/*

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
*/
