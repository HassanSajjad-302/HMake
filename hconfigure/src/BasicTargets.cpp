#include "BasicTargets.hpp"
#include "BuildSystemFunctions.hpp"
#include "Target.hpp"
#include <filesystem>
#include <fstream>

using std::filesystem::create_directories, std::ofstream;

bool IndexInTopologicalSortComparator::operator()(const BTarget *lhs, const BTarget *rhs) const
{
    return lhs->indexInTopologicalSort < rhs->indexInTopologicalSort;
}

BTarget::BTarget(ReportResultTo *reportResultTo_, const ResultType resultType_)
    : reportResultTo{reportResultTo_}, resultType{resultType_}
{
    id = total++;
}

void BTarget::addDependency(BTarget &dependency)
{
    if (dependencies.emplace(&dependency).second)
    {
        dependency.dependents.emplace(this);
        ++dependenciesSize;
        if (!bTarjanNode)
        {
            bTarjanNode = const_cast<TBT *>(tarjanNodesBTargets.emplace(this).first.operator->());
        }
        if (!dependency.bTarjanNode)
        {
            dependency.bTarjanNode = const_cast<TBT *>(tarjanNodesBTargets.emplace(&dependency).first.operator->());
        }
        bTarjanNode->deps.emplace(dependency.bTarjanNode);
    }
}

bool operator<(const BTarget &lhs, const BTarget &rhs)
{
    return lhs.id < rhs.id;
}

bool TarPointerComparator::operator()(const struct CTarget *lhs, const struct CTarget *rhs) const
{
    return lhs->name < rhs->name;
}

CTarget::CTarget(string name_, CTarget &container, const bool hasFile_)
    : name{std::move(name_)}, hasFile{hasFile_}, other(&container)
{
    id = total++;
    cTargets.emplace(id, this);
    if (!container.hasFile)
    {
        print(stderr, "Target {} in file {} has no file. It can't have element target\n", container.name,
              container.targetFileDir);
        exit(EXIT_FAILURE);
    }
    if (hasFile)
    {
        targetFileDir = container.targetFileDir + "/" + name;
    }
    else
    {
        targetFileDir = container.targetFileDir;
    }
    targetFilePaths[targetFileDir].emplace(this);
    if (bsMode == BSMode::CONFIGURE)
    {
        if (container.elements.emplace(this).second)
        {
            addCTargetInTarjanNode();
            container.addCTargetInTarjanNode();
            cTarjanNode->deps.emplace(container.cTarjanNode);
        }
        else
        {
            print(stderr, "Container {} already has the element {}.\n", container.getTargetPointer(),
                  this->getTargetPointer());
            exit(EXIT_FAILURE);
        }
    }
}

CTarget::CTarget(string name_) : name(std::move(name_)), targetFileDir((path(configureDir) / name).string())
{
    id = total++;
    cTargets.emplace(id, this);
    targetFilePaths[targetFileDir].emplace(this);
}

string CTarget::getTargetPointer() const
{
    return targetFileDir + "/" + name;
}

path CTarget::getTargetFilePath() const
{
    return path(targetFileDir) / path("target.json");
}

void CTarget::addCTargetInTarjanNode()
{
    if (!cTarjanNode)
    {
        cTarjanNode = const_cast<TCT *>(tarjanNodesCTargets.emplace(this).first.operator->());
    }
}

void CTarget::setJson()
{
    if (cTargetType == TargetType::EXECUTABLE || cTargetType == TargetType::LIBRARY_STATIC ||
        cTargetType == TargetType::LIBRARY_SHARED)
    {
        static_cast<Target *>(this)->setJsonDerived();
    }
    else if (cTargetType == TargetType::VARIANT)
    {
        static_cast<Variant *>(this)->setJsonDerived();
    }
}

void CTarget::writeJsonFile()
{
    if (hasFile)
    {
        create_directories(targetFileDir);
        path p = getTargetFilePath();
        ofstream(p) << json.dump(4);
    }
    else
    {
        other->json.emplace_back(json);
    }
}

void CTarget::configure()
{
    setJson();
    writeJsonFile();
}

BTarget *CTarget::getBTarget()
{
    // TODO
    switch (cTargetType)
    {
    case TargetType::NOT_ASSIGNED:
        break;
    case TargetType::EXECUTABLE:
        break;
    case TargetType::LIBRARY_STATIC:
        break;
    case TargetType::LIBRARY_SHARED:
        break;
    case TargetType::VARIANT:
        break;
    case TargetType::COMPILE:
        break;
    case TargetType::PREPROCESS:
        break;
    case TargetType::RUN:
        break;
    case TargetType::PLIBRARY_STATIC:
        break;
    case TargetType::PLIBRARY_SHARED:
        break;
    }
    BTarget *target;
    return target;
}

void to_json(Json &j, const CTarget *tar)
{
    j = tar->targetFileDir;
}
