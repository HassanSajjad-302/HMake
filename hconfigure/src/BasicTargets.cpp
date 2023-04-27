
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

RealBTarget::RealBTarget(BTarget *bTarget_, unsigned short round_) : bTarget(bTarget_), round(round_)
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

BTarget::BTarget()
{
    id = total++;
}

BTarget::~BTarget() = default;

string BTarget::getTarjanNodeName() const
{
    return "BTarget " + std::to_string(id);
}

RealBTarget &BTarget::getRealBTarget(unsigned short round)
{
    auto it = realBTargets.try_emplace(round, this, round).first;
    return const_cast<RealBTarget &>(it->second);
}

void BTarget::preSort(Builder &, unsigned short)
{
}

void BTarget::duringSort(Builder &, unsigned short)
{
}

void BTarget::updateBTarget(Builder &, unsigned short)
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
                fmt::format("There exists two targets with name {} and targetFileDir {}", name, targetFileDir));
            throw std::exception();
        }
    }
}

CTarget::CTarget(string name_, CTarget &container, const bool hasFile_)
    : name{std::move(name_)}, other(&container), hasFile{hasFile_}
{
    if (!container.hasFile && hasFile_)
    {
        printErrorMessage(fmt::format(
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
            printErrorMessage(fmt::format("Container {} already has the element {}.\n", container.getTargetPointer(),
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

CTarget::~CTarget() = default;

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

bool CTarget::getSelectiveBuild()
{
    if (bsMode == BSMode::BUILD && !selectiveBuildSet)
    {
        path targetPath = getSubDirForTarget();
        for (; targetPath.root_path() != targetPath; targetPath = (targetPath / "..").lexically_normal())
        {
            std::error_code ec;
            if (equivalent(targetPath, current_path(), ec))
            {
                selectiveBuild = true;
                break;
            }
        }
        selectiveBuildSet = true;
    }
    return selectiveBuild;
}

string CTarget::getTarjanNodeName() const
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
            printErrorMessage(
                fmt::format("Invalid assignment to Json of {} Json type must be array\n", getTargetPointer()));
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

C_Target *CTarget::get_CAPITarget(BSMode)
{
    auto *c_cTarget = new C_CTarget();
    c_cTarget->dir = (new string(getSubDirForTarget()))->c_str();

    auto *c_Target = new C_Target();
    c_Target->type = C_TargetType::C_CONFIGURE_TARGET_TYPE;
    c_Target->object = c_cTarget;
    return c_Target;
}

void to_json(Json &j, const CTarget *tar)
{
    j = tar->targetFileDir;
}

bool operator<(const CTarget &lhs, const CTarget &rhs)
{
    return lhs.id < rhs.id;
}