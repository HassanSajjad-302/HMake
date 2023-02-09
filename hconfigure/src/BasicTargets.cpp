#include "BasicTargets.hpp"
#include "BuildSystemFunctions.hpp"
#include "CppSourceTarget.hpp"
#include <filesystem>
#include <fstream>

using std::filesystem::create_directories, std::ofstream;

bool IndexInTopologicalSortComparator::operator()(const BTarget *lhs, const BTarget *rhs) const
{
    return lhs->indexInTopologicalSort < rhs->indexInTopologicalSort;
}

BTarget::BTarget()
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

void BTarget::updateBTarget(unsigned short round)
{
    fileStatus = FileStatus::UPDATED;
}

void BTarget::printMutexLockRoutine(unsigned short round)
{
}

void BTarget::initializeForBuild()
{
}

void BTarget::checkForPreBuiltAndCacheDir()
{
}

void BTarget::parseModuleSourceFiles(Builder &builder)
{
}

void BTarget::checkForHeaderUnitsCache()
{
}
void BTarget::createHeaderUnits()
{
}
void BTarget::populateSetTarjanNodesSourceNodes(Builder &builder)
{
}

bool operator<(const BTarget &lhs, const BTarget &rhs)
{
    return lhs.id < rhs.id;
}

bool TarPointerComparator::operator()(const struct CTarget *lhs, const struct CTarget *rhs) const
{
    return lhs->name < rhs->name;
}

bool CTargetPointerComparator::operator()(const CTarget *lhs, const CTarget *rhs) const
{
    return (lhs->targetFileDir + lhs->name) < (rhs->targetFileDir + rhs->name);
}

void CTarget::initializeCTarget()
{
    id = total++;
    cTargets.emplace(id, this);
    cTarjanNode = const_cast<TCT *>(tarjanNodesCTargets.emplace(this).first.operator->());
    if (hasFile)
    {
        const auto &[pos, Ok] = containerCTargets.emplace(this);
        if (!Ok)
        {
            print(stderr, "There exists two targets with name {} and targetFileDir {}", name, targetFileDir);
            exit(EXIT_FAILURE);
        }
    }
}

CTarget::CTarget(string name_, CTarget &container, const bool hasFile_)
    : name{std::move(name_)}, hasFile{hasFile_}, other(&container)
{
    if (!container.hasFile)
    {
        print(stderr, "Target {} in file {} has no file. It can't have element target\n", container.name,
              container.targetFileDir);
        exit(EXIT_FAILURE);
    }
    if (hasFile)
    {
        targetFileDir = container.targetFileDir + "/" + name + "/";
    }
    else
    {
        targetFileDir = container.targetFileDir;
    }
    targetFilePaths[targetFileDir].emplace(this);
    initializeCTarget();
    if (bsMode == BSMode::CONFIGURE)
    {
        if (container.elements.emplace(this).second)
        {
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

CTarget::CTarget(string name_)
    : name(std::move(name_)), targetFileDir(path(configureDir).generic_string() + "/" + name + "/")
{
    targetFilePaths[targetFileDir].emplace(this);
    initializeCTarget();
}

string CTarget::getTargetPointer() const
{
    return targetFileDir + "/" + name;
}

path CTarget::getTargetFilePath() const
{
    return path(targetFileDir) / path("target.json");
}

void CTarget::setJson()
{
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
    return nullptr;
}

void CTarget::populateCTargetDependencies()
{
}

void CTarget::addPrivatePropertiesToPublicProperties()
{
}

void to_json(Json &j, const CTarget *tar)
{
    j = tar->targetFileDir;
}
