#include "BasicTargets.hpp"
#include "BuildSystemFunctions.hpp"
#include "CppSourceTarget.hpp"
#include <filesystem>
#include <fstream>

using std::filesystem::create_directories, std::ofstream;

IndexInTopologicalSortComparator::IndexInTopologicalSortComparator(unsigned short round_) : round(round_)
{
}

bool IndexInTopologicalSortComparator::operator()(const BTarget *lhs, const BTarget *rhs) const
{
    return lhs->realBTargets.find(round)->indexInTopologicalSort <
           rhs->realBTargets.find(round)->indexInTopologicalSort;
}

RealBTarget::RealBTarget(unsigned short round_, BTarget *bTarget_)
    : round(round_), bTarget(bTarget_), allDependencies(IndexInTopologicalSortComparator(round_))
{
    bTarjanNode = const_cast<TBT *>(
        tarjanNodesBTargets.emplace(round, set<TBT>()).first->second.emplace(bTarget).first.operator->());
}

void RealBTarget::addDependency(BTarget &dependency)
{
    if (dependencies.emplace(&dependency).second)
    {
        RealBTarget &dependencyRealBTarget = dependency.getRealBTarget(round);
        dependencyRealBTarget.dependents.emplace(bTarget);
        ++dependenciesSize;
        bTarjanNode->deps.emplace(dependencyRealBTarget.bTarjanNode);
    }
}

bool CompareRealBTargetId::operator()(RealBTarget const &lhs, RealBTarget const &rhs) const
{
    return lhs.round < rhs.round;
}

bool CompareRealBTargetId::operator()(unsigned short round, RealBTarget const &rhs) const
{
    return round < rhs.round;
}

bool CompareRealBTargetId::operator()(RealBTarget const &lhs, unsigned short round) const
{
    return lhs.round < round;
}

BTarget::BTarget()
{
    id = total++;
}

string BTarget::getTarjanNodeName()
{
    return "BTarget " + std::to_string(id);
}

RealBTarget &BTarget::getRealBTarget(unsigned short round)
{
    auto it = realBTargets.emplace(round, this).first;
    return const_cast<RealBTarget &>(*it);
}

void BTarget::updateBTarget(unsigned short, Builder &)
{
}

void BTarget::printMutexLockRoutine(unsigned short)
{
}

void BTarget::preSort(Builder &, unsigned short)
{
}

void BTarget::duringSort(Builder &, unsigned short)
{
}

bool operator<(const BTarget &lhs, const BTarget &rhs)
{
    return lhs.id < rhs.id;
}

bool TarPointerComparator::operator()(const CTarget *lhs, const CTarget *rhs) const
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
    targetPointers<CTarget>.emplace(this);
    cTarjanNode = const_cast<TCT *>(tarjanNodesCTargets.emplace(this).first.operator->());
    if (hasFile)
    {
        const auto &[pos, Ok] = cTargetsSameFileAndNameCheck.emplace(this);
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
    if (!container.hasFile && hasFile_)
    {
        print(stderr, "Target {} in file {} has no file. It can't have element target\n", container.name,
              container.targetFileDir);
        exit(EXIT_FAILURE);
    }
    if (hasFile)
    {
        targetFileDir = container.targetFileDir + name + "/";
    }
    else
    {
        targetFileDir = container.targetFileDir;
    }
    targetSubDirectories.emplace(getSubDirForTarget());
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
    targetSubDirectories.emplace(getSubDirForTarget());
    initializeCTarget();
}

string CTarget::getTargetPointer() const
{
    return other ? other->getTargetPointer() + (hasFile ? "" : "/") + name + "/" : targetFileDir;
}

path CTarget::getTargetFilePath() const
{
    return path(targetFileDir) / path("target.json");
}

string CTarget::getSubDirForTarget() const
{
    return other ? other->getSubDirForTarget() + name + "/" : targetFileDir;
}

string CTarget::getTarjanNodeName()
{
    return "CTarget " + getSubDirForTarget();
}

void CTarget::setJson()
{
    json[0][JConsts::name] = name;
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
        create_directories(getSubDirForTarget());
        if (!other->json.is_array())
        {
            print("Invalid assignment to Json of {} Json type must be array\n", getTargetPointer());
            exit(EXIT_FAILURE);
        }
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

void to_json(Json &j, const CTarget *tar)
{
    j = tar->targetFileDir;
}
