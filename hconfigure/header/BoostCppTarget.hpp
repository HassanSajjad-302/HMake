
#ifndef BOOSTCPPTARGET_HPP
#define BOOSTCPPTARGET_HPP

#include "Configuration.hpp"
#include "CppSourceTarget.hpp"
#include "DSC.hpp"

using std::filesystem::directory_iterator;
enum class BoostExampleOrTestType : char
{
    RUN_TEST,
    COMPILE_TEST,
    COMPILE_FAIL_TEST,
    LINK_TEST,
    LINK_FAIL_TEST,
    EXAMPLE,
    RUN_EXAMPLE,
};

union BoostTestTargetType {
    DSC<CppSourceTarget> *dscTarget;
    CppSourceTarget *cppTarget;
};

struct ExampleOrTest
{
    // I think union should be used here for the cases where there is only CppSourceTarget and no complete
    // DSC<CppSourceTarget>.
    BoostTestTargetType testTarget;
    BoostExampleOrTestType targetType;
};

enum class IteratorTargetType : char
{
    DSC_CPP,
    CPP,
    LINK,
};

#ifdef BUILD_MODE
#define BUILD_INLINE inline
#else
#define BUILD_INLINE
#endif

class BoostCppTarget : TargetCache
{
    template <BoostExampleOrTestType boostExampleOrTestType, IteratorTargetType iteratorTargetType> struct GenericBase
    {
        const vector<ExampleOrTest> &examplesOrTests;
        const ExampleOrTest *exampleOrTest;

        explicit GenericBase(const vector<ExampleOrTest> &examplesOrTests_, const ExampleOrTest *exampleOrTest_);
        auto &operator*() const;
        bool operator!=(const GenericBase &other) const;
        GenericBase end() const;
    };

    template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm>
    struct Get : public GenericBase<EOT, iteratorTargetType>
    {
        explicit Get(const vector<ExampleOrTest> &examplesOrTests_, const ExampleOrTest *exampleOrTest_);
        Get begin();
        Get operator++();
    };

    template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm>
    struct GetEnds : public GenericBase<EOT, iteratorTargetType>
    {
        const char *endsWith;
        explicit GetEnds(const char *endsWith_, const vector<ExampleOrTest> &examplesOrTests_,
                         const ExampleOrTest *exampleOrTest_);
        GetEnds begin();
        GetEnds operator++();
    };

    template <BoostExampleOrTestType EOT, bool addInConfigCache> struct Add
    {
        void operator()(BoostCppTarget &target, string_view sourceDir, string_view fileName);
    };

    template <BoostExampleOrTestType EOT> struct Add<EOT, false>
    {
        void operator()(BoostCppTarget &target, string_view sourceDir, string_view fileName);
    };

    template <BoostExampleOrTestType EOT, bool addInConfigCache> struct AddEnds
    {
        void operator()(BoostCppTarget &target, string_view innerBuildDirName, string_view sourceDir,
                        string_view fileName);
    };

    template <BoostExampleOrTestType EOT> struct AddEnds<EOT, false>
    {
        void operator()(BoostCppTarget &target, string_view innerBuildDirName, string_view sourceDir,
                        string_view fileName);
    };

  public:
    Configuration *configuration = nullptr;
    BTarget *testTarget = nullptr;
    BTarget *examplesTarget = nullptr;
    DSC<CppSourceTarget> &mainTarget;
    vector<ExampleOrTest> examplesOrTests;

    BoostCppTarget(const string &name, Configuration *configuration_, bool headerOnly, bool createTestsTarget = false,
                   bool createExamplesTarget = false);
    ~BoostCppTarget();

    template <BoostExampleOrTestType EOT>
    void getTargetFromConfiguration(string_view name, string_view buildCacheFilesDirPath, const Node *node);
    template <BoostExampleOrTestType EOT> bool getExplicitBuilding() const;
    template <BoostExampleOrTestType EOT> static string getInnerBuildDirExcludingFileName();
    template <BoostExampleOrTestType EOT>
    static string getInnerBuildDirExcludingFileName(string_view innerBuildDirName);

    template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm = BSMode::BUILD> auto get();
    template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm = BSMode::BUILD>
    auto getEndsWith(const char *endsWith);

    // TODO
    //  Change ends_with to contains.

    // TODO
    // Currently, not dealing with different kinds of tests and their results and not combining them either.
    template <BoostExampleOrTestType EOT> BUILD_INLINE BoostCppTarget &addDir(string_view sourceDir);
    template <BoostExampleOrTestType EOT>
    BUILD_INLINE BoostCppTarget &addDirEndsWith(string_view sourceDir, string_view innerBuildDirName);

    template <BoostExampleOrTestType EOT>
    BoostCppTarget &add(string_view sourceDir, const string_view *fileName, uint64_t arraySize);
    template <BoostExampleOrTestType EOT>
    BoostCppTarget &addEndsWith(string_view innerBuildDirName, string_view sourceDir, const string_view *fileName,
                                uint64_t arraySize);

    template <BoostExampleOrTestType EOT> BoostCppTarget &add(string_view sourceDir, string_view fileName);
    template <BoostExampleOrTestType EOT>
    BoostCppTarget &addEndsWith(string_view innerBuildDirName, string_view sourceDir, string_view fileName);
};

template <BoostExampleOrTestType boostExampleOrTestType, IteratorTargetType boostTargetType>
BoostCppTarget::GenericBase<boostExampleOrTestType, boostTargetType>::GenericBase(
    const vector<ExampleOrTest> &examplesOrTests_, const ExampleOrTest *exampleOrTest_)
    : examplesOrTests(examplesOrTests_), exampleOrTest(exampleOrTest_)
{
}

template <BoostExampleOrTestType boostExampleOrTestType, IteratorTargetType iteratorTargetType>
auto &BoostCppTarget::GenericBase<boostExampleOrTestType, iteratorTargetType>::operator*() const
{
    if constexpr ((boostExampleOrTestType == BoostExampleOrTestType::COMPILE_TEST ||
                   boostExampleOrTestType == BoostExampleOrTestType::COMPILE_FAIL_TEST) &&
                  iteratorTargetType == IteratorTargetType::LINK)
    {
        static_assert(false && "IteratorTargetType::LINK is not available when the BoostExampleOrTestType is "
                               "COMPILE_TEST or COMPILE_FAIL_TEST");
    }

    if constexpr (iteratorTargetType == IteratorTargetType::DSC_CPP)
    {
        return *exampleOrTest->testTarget.cppTarget;
    }
    else if constexpr (iteratorTargetType == IteratorTargetType::CPP)
    {
        return exampleOrTest->testTarget.dscTarget->getSourceTarget();
    }
    else
    {
        return exampleOrTest->testTarget.dscTarget->getLinkOrArchiveTarget();
    }
}

template <BoostExampleOrTestType boostExampleOrTestType, IteratorTargetType boostTargetType>
bool BoostCppTarget::GenericBase<boostExampleOrTestType, boostTargetType>::operator!=(const GenericBase &other) const
{
    return exampleOrTest != other.exampleOrTest;
}

template <BoostExampleOrTestType boostExampleOrTestType, IteratorTargetType boostTargetType>
BoostCppTarget::GenericBase<boostExampleOrTestType, boostTargetType> BoostCppTarget::GenericBase<
    boostExampleOrTestType, boostTargetType>::end() const
{
    if (!exampleOrTest)
    {
        return GenericBase(examplesOrTests, nullptr);
    }
    return GenericBase(examplesOrTests, examplesOrTests.begin().operator->() + examplesOrTests.size());
}

template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm>
BoostCppTarget::Get<EOT, iteratorTargetType, bsm>::Get(const vector<ExampleOrTest> &examplesOrTests_,
                                                       const ExampleOrTest *exampleOrTest_)
    : GenericBase<EOT, iteratorTargetType>(examplesOrTests_, exampleOrTest_)
{
}

template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm>
BoostCppTarget::Get<EOT, iteratorTargetType, bsm> BoostCppTarget::Get<EOT, iteratorTargetType, bsm>::begin()
{
    auto &examplesOrTests = GenericBase<EOT, iteratorTargetType>::examplesOrTests;
    auto &exampleOrTest = GenericBase<EOT, iteratorTargetType>::exampleOrTest;

    const ExampleOrTest *exampleOrTestLocal = nullptr;
    if constexpr (bsMode == bsm)
    {
        for (const ExampleOrTest &exampleOrTest_ : examplesOrTests)
        {
            if (exampleOrTest_.targetType == EOT)
            {
                exampleOrTestLocal = &exampleOrTest_;
                break;
            }
        }
        // exampleOrTest is nullptr in the constructor, so it is assigned here.
        exampleOrTest = exampleOrTestLocal;
    }
    return Get(examplesOrTests, exampleOrTestLocal);
}

template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm>
BoostCppTarget::Get<EOT, iteratorTargetType, bsm> BoostCppTarget::Get<EOT, iteratorTargetType, bsm>::operator++()
{
    auto &examplesOrTests = GenericBase<EOT, iteratorTargetType>::examplesOrTests;
    auto &exampleOrTest = GenericBase<EOT, iteratorTargetType>::exampleOrTest;

    if constexpr (bsMode == bsm)
    {
        ++exampleOrTest;
        for (; exampleOrTest != examplesOrTests.begin().operator->() + examplesOrTests.size(); ++exampleOrTest)
        {
            if (exampleOrTest->targetType == EOT)
            {
                return Get(examplesOrTests, exampleOrTest);
            }
        }
    }
    return Get(examplesOrTests, nullptr);
}

template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm>
BoostCppTarget::GetEnds<EOT, iteratorTargetType, bsm>::GetEnds(const char *endsWith_,
                                                               const vector<ExampleOrTest> &examplesOrTests_,
                                                               const ExampleOrTest *exampleOrTest_)
    : GenericBase<EOT, iteratorTargetType>(examplesOrTests_, exampleOrTest_), endsWith(endsWith_)
{
}

template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm>
BoostCppTarget::GetEnds<EOT, iteratorTargetType, bsm> BoostCppTarget::GetEnds<EOT, iteratorTargetType, bsm>::begin()
{
    auto &examplesOrTests = GenericBase<EOT, iteratorTargetType>::examplesOrTests;
    auto &exampleOrTest = GenericBase<EOT, iteratorTargetType>::exampleOrTest;

    const ExampleOrTest *exampleOrTestLocal = nullptr;
    if constexpr (bsMode == bsm)
    {
        string finalEndString;
        if constexpr (EOT == BoostExampleOrTestType::EXAMPLE || EOT == BoostExampleOrTestType::RUN_EXAMPLE)
        {
            finalEndString = "Examples";
        }
        else
        {
            finalEndString = "Tests";
        }
        finalEndString += slashc;
        finalEndString += endsWith;
        lowerCasePStringOnWindows(finalEndString.data(), finalEndString.size());
        for (const ExampleOrTest &exampleOrTest_ : examplesOrTests)
        {
            if constexpr (EOT == BoostExampleOrTestType::COMPILE_TEST ||
                          EOT == BoostExampleOrTestType::COMPILE_FAIL_TEST)
            {
                if (exampleOrTest_.targetType == EOT &&
                    exampleOrTest_.testTarget.cppTarget->name.contains(finalEndString))
                {
                    exampleOrTestLocal = &exampleOrTest_;
                    break;
                }
            }
            else
            {
                if (exampleOrTest_.targetType == EOT &&
                    exampleOrTest_.testTarget.dscTarget->getSourceTarget().name.contains(finalEndString))
                {
                    exampleOrTestLocal = &exampleOrTest_;
                    break;
                }
            }
        }
        exampleOrTest = exampleOrTestLocal;
    }
    return GetEnds(std::move(endsWith), examplesOrTests, exampleOrTestLocal);
}

template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm>
BoostCppTarget::GetEnds<EOT, iteratorTargetType, bsm> BoostCppTarget::GetEnds<EOT, iteratorTargetType,
                                                                              bsm>::operator++()
{
    auto &examplesOrTests = GenericBase<EOT, iteratorTargetType>::examplesOrTests;
    auto &exampleOrTest = GenericBase<EOT, iteratorTargetType>::exampleOrTest;

    if constexpr (bsMode == bsm)
    {
        ++exampleOrTest;
        string finalEndString;
        if constexpr (EOT == BoostExampleOrTestType::EXAMPLE || EOT == BoostExampleOrTestType::RUN_EXAMPLE)
        {
            finalEndString = "Examples";
        }
        else
        {
            finalEndString = "Tests";
        }
        finalEndString += slashc;
        finalEndString += endsWith;
        lowerCasePStringOnWindows(finalEndString.data(), finalEndString.size());
        for (; exampleOrTest != examplesOrTests.begin().operator->() + examplesOrTests.size(); ++exampleOrTest)
        {
            if (exampleOrTest->targetType == EOT &&
                exampleOrTest->testTarget.dscTarget->getSourceTarget().name.contains(finalEndString))
            {
                return GetEnds(std::move(endsWith), examplesOrTests, exampleOrTest);
            }
        }
    }
    return GetEnds(std::move(endsWith), examplesOrTests, nullptr);
}

template <BoostExampleOrTestType EOT, bool addInConfigCache>
void BoostCppTarget::Add<EOT, addInConfigCache>::operator()(BoostCppTarget &target, string_view sourceDir,
                                                            string_view fileName)
{
    const string configurationNamePlusTargetName = target.mainTarget.getPrebuiltBasicTarget().name + slashc;

    const string buildCacheFilesDirPath =
        configurationNamePlusTargetName + target.getInnerBuildDirExcludingFileName<EOT>();
    const string pushName =
        target.getInnerBuildDirExcludingFileName<EOT>() + slashc + getNameBeforeLastPeriod(fileName);
    const string name = configurationNamePlusTargetName + pushName;

    target.buildOrConfigCacheCopy.PushBack(static_cast<uint8_t>(EOT), target.cacheAlloc);
    target.buildOrConfigCacheCopy.PushBack(
        Value(kStringType).SetString(pushName.c_str(), pushName.size(), target.cacheAlloc), target.cacheAlloc);

    const Node *node = Node::getNodeFromNonNormalizedString(string(sourceDir) + slashc + string(fileName), true);
    target.getTargetFromConfiguration<EOT>(name, buildCacheFilesDirPath, node);
}

template <BoostExampleOrTestType EOT>
void BoostCppTarget::Add<EOT, false>::operator()(BoostCppTarget &target, string_view sourceDir, string_view fileName)
{
    const string configurationNamePlusTargetName = target.mainTarget.getPrebuiltBasicTarget().name + slashc;

    const string buildCacheFilesDirPath =
        configurationNamePlusTargetName + target.getInnerBuildDirExcludingFileName<EOT>();
    const string pushName =
        target.getInnerBuildDirExcludingFileName<EOT>() + slashc + getNameBeforeLastPeriod(fileName);
    const string name = configurationNamePlusTargetName + pushName;

    const Node *node = Node::getNodeFromNonNormalizedString(string(sourceDir) + slashc + string(fileName), true);
    target.getTargetFromConfiguration<EOT>(name, buildCacheFilesDirPath, node);
}

template <BoostExampleOrTestType EOT, bool addInConfigCache>
void BoostCppTarget::AddEnds<EOT, addInConfigCache>::operator()(BoostCppTarget &target, string_view innerBuildDirName,
                                                                string_view sourceDir, string_view fileName)
{
    const string configurationNamePlusTargetName = target.mainTarget.getPrebuiltBasicTarget().name + slashc;

    const string buildCacheFilesDirPath =
        configurationNamePlusTargetName + target.getInnerBuildDirExcludingFileName<EOT>(innerBuildDirName);
    const string pushName =
        target.getInnerBuildDirExcludingFileName<EOT>(innerBuildDirName) + slashc + getNameBeforeLastPeriod(fileName);
    const string name = configurationNamePlusTargetName + pushName;

    target.buildOrConfigCacheCopy.PushBack(static_cast<uint8_t>(EOT), target.cacheAlloc);
    target.buildOrConfigCacheCopy.PushBack(
        Value(kStringType).SetString(pushName.c_str(), pushName.size(), target.cacheAlloc), target.cacheAlloc);

    const Node *node = Node::getNodeFromNonNormalizedString(string(sourceDir) + slashc + string(fileName), true);
    target.getTargetFromConfiguration<EOT>(name, buildCacheFilesDirPath, node);
}

template <BoostExampleOrTestType EOT>
void BoostCppTarget::AddEnds<EOT, false>::operator()(BoostCppTarget &target, string_view innerBuildDirName,
                                                     string_view sourceDir, string_view fileName)
{
    const string configurationNamePlusTargetName = target.mainTarget.getPrebuiltBasicTarget().name + slashc;

    const string buildCacheFilesDirPath =
        configurationNamePlusTargetName + target.getInnerBuildDirExcludingFileName<EOT>(innerBuildDirName);
    const string pushName =
        target.getInnerBuildDirExcludingFileName<EOT>(innerBuildDirName) + slashc + getNameBeforeLastPeriod(fileName);
    const string name = configurationNamePlusTargetName + pushName;

    const Node *node = Node::getNodeFromNonNormalizedString(string(sourceDir) + slashc + string(fileName), true);
    target.getTargetFromConfiguration<EOT>(name, buildCacheFilesDirPath, node);
}

template <BoostExampleOrTestType EOT>
void BoostCppTarget::getTargetFromConfiguration(string_view name, string_view buildCacheFilesDirPath, const Node *node)
{
    if constexpr (EOT == BoostExampleOrTestType::COMPILE_TEST)
    {
        configuration
            ->getCppObjectNoNameAddStdTarget(getExplicitBuilding<EOT>(), string(buildCacheFilesDirPath), string(name))
            .privateDeps(&mainTarget.getSourceTarget())
            .moduleFiles(node->filePath);
    }
    else
    {
        configuration->getCppExeDSCNoName(getExplicitBuilding<EOT>(), string(buildCacheFilesDirPath), string(name))
            .privateLibraries(&mainTarget)
            .getSourceTarget()
            .moduleFiles(node->filePath);
    }
}

template <BoostExampleOrTestType EOT> bool BoostCppTarget::getExplicitBuilding() const
{
    if constexpr (EOT == BoostExampleOrTestType::EXAMPLE || EOT == BoostExampleOrTestType::RUN_EXAMPLE)
    {
        return configuration->evaluate(ExamplesExplicit::YES);
    }
    else
    {
        return configuration->evaluate(TestsExplicit::YES);
    }
}

template <BoostExampleOrTestType EOT> string BoostCppTarget::getInnerBuildDirExcludingFileName()
{
    string str;
    if constexpr (EOT == BoostExampleOrTestType::EXAMPLE || EOT == BoostExampleOrTestType::RUN_EXAMPLE)
    {
        str = "Examples";
    }
    else
    {
        str = "Tests";
    }
    return str;
}

template <BoostExampleOrTestType EOT>
string BoostCppTarget::getInnerBuildDirExcludingFileName(string_view innerBuildDirName)
{
    string str;
    if constexpr (EOT == BoostExampleOrTestType::EXAMPLE || EOT == BoostExampleOrTestType::RUN_EXAMPLE)
    {
        str = "Examples";
    }
    else
    {
        str = "Tests";
    }
    str += slashc;
    str += innerBuildDirName;
    return str;
}

template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm> auto BoostCppTarget::get()
{
    return Get<EOT, iteratorTargetType, bsm>(examplesOrTests, nullptr);
}

template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm>
auto BoostCppTarget::getEndsWith(const char *endsWith = "")
{
    return GetEnds<EOT, iteratorTargetType, bsm>(endsWith, examplesOrTests, nullptr);
}

template <BoostExampleOrTestType EOT> BoostCppTarget &BoostCppTarget::addDir(string_view sourceDir)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(BuildTests::YES))
        {
            for (const auto &k : directory_iterator(path(srcNode->filePath + string(sourceDir))))
            {
                if (k.path().extension() == ".cpp")
                {
                    Add<EOT, true>{}(*this, k.path().parent_path().string(), k.path().filename().string());
                }
            }
        }
    }
    return *this;
}

template <BoostExampleOrTestType EOT>
BoostCppTarget &BoostCppTarget::addDirEndsWith(string_view sourceDir, string_view innerBuildDirName)
{
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(BuildTests::YES))
        {
            for (const auto &k : directory_iterator(path(srcNode->filePath + string(sourceDir))))
            {
                if (k.path().extension() == ".cpp")
                {
                    AddEnds<EOT, true>{}(*this, innerBuildDirName, k.path().parent_path().string(),
                                         k.path().filename().string());
                }
            }
        }
    }
    return *this;
}

template <BoostExampleOrTestType EOT>
BoostCppTarget &BoostCppTarget::add(string_view sourceDir, const string_view *fileName, uint64_t arraySize)
{
    for (uint64_t i = 0; i < arraySize; ++i)
    {
        Add<EOT, false>{}(*this, sourceDir, fileName[i]);
    }
    return *this;
}

template <BoostExampleOrTestType EOT>
BoostCppTarget &BoostCppTarget::addEndsWith(string_view innerBuildDirName, string_view sourceDir,
                                            const string_view *fileName, uint64_t arraySize)
{
    for (uint64_t i = 0; i < arraySize; ++i)
    {
        AddEnds<EOT, false>{}(*this, innerBuildDirName, sourceDir, fileName[i]);
    }
    return *this;
}

template <BoostExampleOrTestType EOT> BoostCppTarget &BoostCppTarget::add(string_view sourceDir, string_view fileName)
{
    Add<EOT, false>{}(*this, sourceDir, fileName);
    return *this;
}

template <BoostExampleOrTestType EOT>
BoostCppTarget &BoostCppTarget::addEndsWith(string_view innerBuildDirName, string_view sourceDir, string_view fileName)
{
    Add<EOT, false>{}(*this, innerBuildDirName, sourceDir, fileName);
    return *this;
}

#endif // BOOSTCPPTARGET_HPP
