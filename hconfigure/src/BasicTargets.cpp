
#ifdef USE_HEADER_UNITS
import "BasicTargets.hpp";
import "Builder.hpp";
import "BuildSystemFunctions.hpp";
import "CppSourceTarget.hpp";
import <filesystem>;
import <fstream>;
#else
#include "BasicTargets.hpp"
#include "BuildSystemFunctions.hpp"
#include "Builder.hpp"
#include "CppSourceTarget.hpp"
#include <filesystem>
#include <fstream>
#endif
using std::filesystem::create_directories, std::ofstream, std::filesystem::current_path, std::mutex, std::lock_guard;

StaticInitializationTarjanNodesBTargets::StaticInitializationTarjanNodesBTargets()
{
    for (unsigned short i = 0; i < 3; ++i)
    {
        tarjanNodesBTargets.emplace_back();
        tarjanNodesBTargetsMutexes.emplace_back(new mutex());
    }
}

bool IndexInTopologicalSortComparatorRoundZero::operator()(const BTarget *lhs, const BTarget *rhs) const
{
    return const_cast<BTarget *>(lhs)->realBTargets[0].indexInTopologicalSort >
           const_cast<BTarget *>(rhs)->realBTargets[0].indexInTopologicalSort;
}

RealBTarget::RealBTarget(BTarget *bTarget_, const unsigned short round_) : TBT{bTarget_}, bTarget(bTarget_), round(round_)
{
    std::lock_guard<std::mutex> lk(*(tarjanNodesBTargetsMutexes[round]));

    // Memory Not Released
    tarjanNodesBTargets[round].emplace_back(this);
}

/*void RealBTarget::addInGlobalTarjanNodes()
{
    std::lock_guard<std::mutex> lk(*(tarjanNodesBTargetsMutexes[round]));
    if (!addedInGlobalTarjanNodes)
    {
        addedInGlobalTarjanNodes = true;
        // Memory Not Released
        tarjanNodesBTargets[round].emplace_back(this);
    }
}*/

BTarget::BTarget() : realBTargets{RealBTarget(this, 0), RealBTarget(this, 1), RealBTarget(this, 2)}
{
    id = total++;
}

BTarget::~BTarget()
{
    int a = 3;
}

pstring BTarget::getTarjanNodeName() const
{
    return "BTarget " + to_pstring(id);
}

BTargetType BTarget::getBTargetType() const
{
    return static_cast<BTargetType>(0);
}

void BTarget::assignFileStatusToDependents(RealBTarget &realBTarget) const
{
    for (auto &[dependent, bTargetDepType] : realBTarget.dependents)
    {
        if (bTargetDepType == BTargetDepType::FULL)
        {
            dependent->fileStatus.store(true);
        }
    }
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
    // Memory Not Released
    tarjanNodesCTargets.emplace_back(this);
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
    targetSubDir = other ? (other->targetSubDir + name + slashc) : targetFileDir;
}

CTarget::CTarget(pstring name_, CTarget &container, const bool hasFile_)
    : TCT{this}, name{std::move(name_)}, other(&container), hasFile{hasFile_}
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
        targetFileDir = container.targetFileDir + name + slashc;
    }
    else
    {
        targetFileDir = container.targetFileDir;
    }
    initializeCTarget();
    targetSubDirectories.emplace(targetSubDir);
    if (bsMode == BSMode::CONFIGURE)
    {
        if (container.elements.emplace(this).second)
        {
            deps.emplace(&container);
        }
        else
        {
            printErrorMessage(fmt::format("Container {} already has the element {}.\n", container.getTargetPointer(),
                                          this->getTargetPointer()));
            throw std::exception();
        }
    }
}

CTarget::CTarget(pstring name_)
    : TCT{this}, name(std::move(name_)), targetFileDir((path(configureDir).*toPStr)() + slashc + name + slashc)
{
    initializeCTarget();
    targetSubDirectories.emplace(targetSubDir);
}

CTarget::~CTarget() = default;

pstring CTarget::getTargetPointer() const
{
    if (other)
    {
        if (hasFile)
        {
            return other->getTargetPointer() + slashc + name + slashc;
        }
        return other->getTargetPointer() + name + slashc;
    }
    return targetFileDir;
}

path CTarget::getTargetFilePath() const
{
    return path(targetFileDir) / path("target.json");
}

// selectiveBuild is set for the children if hbuild is executed in parent directory. Uses by the Builder::Builder
bool CTarget::getSelectiveBuild()
{
    if (bsMode == BSMode::BUILD && !selectiveBuildSet)
    {
        path targetPath = targetSubDir;
        const path compare = current_path();
        for (; targetPath.root_path() != targetPath; targetPath = (targetPath / "..").lexically_normal())
        {
            std::error_code ec;
            if (equivalent(targetPath, compare, ec))
            {
                selectiveBuild = true;
                break;
            }
        }
        selectiveBuildSet = true;
    }
    return selectiveBuild;
}

// selectiveBuild is set for the parent if hbuild is executed in child directory. Used in hmake.cpp to rule out other
// configurations specifications
bool CTarget::getSelectiveBuildChildDir()
{
    if (bsMode == BSMode::BUILD && !selectiveBuildSet)
    {
        const path targetPath = targetSubDir;
        path compare = current_path();
        for (; compare.root_path() != compare; compare = (compare / "..").lexically_normal())
        {
            std::error_code ec;
            if (equivalent(targetPath, compare, ec))
            {
                selectiveBuild = true;
                break;
            }
        }
        selectiveBuildSet = true;
    }
    return selectiveBuild;
}

pstring CTarget::getTarjanNodeName() const
{
    return "CTarget " + targetSubDir;
}

void CTarget::setJson()
{
    // json[0][JConsts::name] = name;
}

void CTarget::writeJsonFile()
{
    if (hasFile)
    {
        create_directories(targetFileDir);
        if (!json.empty())
        {
            const path p = getTargetFilePath();
            ofstream(p) << json.dump(4);
        }
    }
    else
    {
        create_directories(targetSubDir);
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
    c_cTarget->dir = targetSubDir.c_str();

    auto *c_Target = new C_Target();
    c_Target->type = C_CONFIGURE_TARGET_TYPE;
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