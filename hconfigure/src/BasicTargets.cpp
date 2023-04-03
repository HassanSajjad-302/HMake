
#ifdef USE_HEADER_UNITS
import "BasicTargets.hpp";
import "BuildSystemFunctions.hpp";
import "CppSourceTarget.hpp";
import <filesystem>;
import <fstream>;
#else
#include "BasicTargets.hpp"
#include "BuildSystemFunctions.hpp"
#include "CppSourceTarget.hpp"
#include <filesystem>
#include <fstream>
#endif
using std::filesystem::create_directories, std::ofstream, std::filesystem::current_path;

/*IndexInTopologicalSortComparator::IndexInTopologicalSortComparator(unsigned short round_) : round(round_)
{
}

bool IndexInTopologicalSortComparator::operator()(const BTarget *lhs, const BTarget *rhs) const
{
    return lhs->realBTargets.find(round)->indexInTopologicalSort <
           rhs->realBTargets.find(round)->indexInTopologicalSort;
}*/

RealBTarget::RealBTarget(unsigned short round_, BTarget *bTarget_) : round(round_), bTarget(bTarget_)
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

bool CompareRealBTargetRound::operator()(RealBTarget const &lhs, RealBTarget const &rhs) const
{
    return lhs.round < rhs.round;
}

bool CompareRealBTargetRound::operator()(unsigned short round, RealBTarget const &rhs) const
{
    return round < rhs.round;
}

bool CompareRealBTargetRound::operator()(RealBTarget const &lhs, unsigned short round) const
{
    return lhs.round < round;
}

BTarget::BTarget()
{
    id = total++;
}

string BTarget::getTarjanNodeName() const
{
    return "BTarget " + std::to_string(id);
}

RealBTarget &BTarget::getRealBTarget(unsigned short round)
{
    auto it = realBTargets.emplace(round, this).first;
    return const_cast<RealBTarget &>(*it);
}

void BTarget::updateBTarget(Builder &, unsigned short)
{
}

void BTarget::preSort(Builder &, unsigned short)
{
}

void BTarget::duringSort(Builder &, unsigned short, unsigned int)
{
}

bool operator<(const BTarget &lhs, const BTarget &rhs)
{
    return lhs.id < rhs.id;
}

bool CTargetPointerComparator::operator()(const CTarget *lhs, const CTarget *rhs) const
{
    return lhs->name < rhs->name;
}

void CTarget::initializeCTarget()
{
    id = total++;
    targetPointers<CTarget>.emplace(this);
    cTarjanNode = const_cast<TCT *>(tarjanNodesCTargets.emplace(this).first.operator->());
    if (hasFile)
    {
        const auto &[pos, Ok] = cTargetsSameFileAndNameCheck.emplace(name, targetFileDir);
        if (!Ok)
        {
            printErrorMessage(
                format("There exists two targets with name {} and targetFileDir {}", name, targetFileDir));
            throw std::exception();
        }
    }
}

CTarget::CTarget(string name_, CTarget &container, const bool hasFile_)
    : name{std::move(name_)}, hasFile{hasFile_}, other(&container)
{
    if (!container.hasFile && hasFile_)
    {
        printErrorMessage(format(
            "Container Target\n{}\nwith no file can't have target\n{}\nwith file. It can't have element target\n",
            container.name, name));
        throw std::exception();
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
            printErrorMessage(format("Container {} already has the element {}.\n", container.getTargetPointer(),
                                     this->getTargetPointer()));
            throw std::exception();
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
    return other ? (other->getSubDirForTarget() + name + "/") : targetFileDir;
}

bool CTarget::isCTargetInSelectedSubDirectory() const
{
    if (bsMode == BSMode::BUILD)
    {
        path targetPath = getSubDirForTarget();
        for (; targetPath.root_path() != targetPath; targetPath = (targetPath / "..").lexically_normal())
        {
            std::error_code ec;
            if (equivalent(targetPath, current_path(), ec))
            {
                return true;
            }
        }
    }
    return false;
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
            printErrorMessage(format("Invalid assignment to Json of {} Json type must be array\n", getTargetPointer()));
            throw std::exception();
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
