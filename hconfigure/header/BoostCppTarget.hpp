
#ifndef BOOSTCPPTARGET_HPP
#define BOOSTCPPTARGET_HPP

#include "Configuration.hpp"
#include "CppTarget.hpp"
#include "DSC.hpp"

void removeTroublingHu(const string_view *headerUnitsJsonDirs, uint64_t headerUnitsJsonDirsSize,
                       const string_view *headerUnitsJsonEntry, uint64_t headerUnitsJsonEntrySize);
using std::filesystem::directory_iterator;

// Currently, the tests are not run after compilation, neither does the build-system compares the output.
// Build-system does not support the fail-tests either.
enum class BoostExampleOrTestType : uint8_t
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
    DSC<CppTarget> *dscTarget;
    CppTarget *cppTarget;
};

struct ExampleOrTest
{
    // I think union should be used here for the cases where there is only CppTarget and no complete
    // DSC<CppTarget>.
    BoostTestTargetType testTarget;
    BoostExampleOrTestType targetType;
};

enum class IteratorTargetType : uint8_t
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

// This class represents a Boost library. It has a mainTarget which represents the boost library.
// And has functions to define Tests and Examples based on BoostExampleOrTestType. (These functions consider the values
// of buildTests, buildExamples, testsExplicit and examplesExplicit of Configuration class).
// This also has testTarget and exampleTarget which you can run to build all the tests and examples.
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
    DSC<CppTarget> &mainTarget;
    vector<ExampleOrTest> examplesOrTests;
    vector<DSC<CppTarget> *> dscTestDepsPrivate;
    vector<CppTarget *> cppTestDepsPrivate;
    vector<char> configBuffer;
    uint32_t testsOrExamplesCount = 0;
    flat_hash_map<string, Node *> testReqHeaderFiles;
    flat_hash_map<string, Node *> testReqHeaderUnits;

    BoostCppTarget(const string &name, Configuration *configuration_, bool headerOnly = true, bool hasBigHeader = true,
                   bool createTestsTarget = false, bool createExamplesTarget = false);

    // When using addDir or addDirEndsWith, if you have also added dependencies for Tests or Examples via the
    // privateTestDeps function, ensure that this function is called after all addDir* calls. The addDir* methods do not
    // perform any actions at build time; they simply record the directory files during configuration time. Later, the
    // BoostCppTarget constructor loads these targets into the testsOrExamples variable at build time. The following
    // call will then iterate over testsOrExamples and add the specified targets as deps.
    BoostCppTarget &assignPrivateTestDeps();
    void copyConfigCache();

    // This function adds a dependency for all subsequent Tests or Examples target. Any add function
    // call (non addDir) will add these targets as dependency. While addDir functions should later-on call
    // assignPrivateTestDeps function.
    template <typename T, typename... U> BoostCppTarget &privateTestDeps(T &dep_, U &&...deps_);
    template <typename T, typename... U> BoostCppTarget &publicDeps(T &dep_, U &&...deps_);
    template <typename T, typename... U> BoostCppTarget &privateDeps(T &dep_, U &&...deps_);
    template <typename T, typename... U> BoostCppTarget &interfaceDeps(T &dep_, U &&...deps_);
    template <typename T, typename... U> BoostCppTarget &deps(DepType depType, T &dep_, U &&...deps_);

    template <BoostExampleOrTestType EOT>
    void getTargetFromConfiguration(string_view name, Node *myBuildDir, const string &filePath) const;
    template <BoostExampleOrTestType EOT> bool getExplicitBuilding() const;
    template <BoostExampleOrTestType EOT> static string getInnerBuildDirExcludingFileName();
    template <BoostExampleOrTestType EOT>
    static string getInnerBuildDirExcludingFileName(string_view innerBuildDirName);

    // This function can be used to iterate over the addDir targets to define different properties at build-time.
    // While the targets added using simple add function can be modified manually since we know the source-files array
    // corresponding to these targets at build-time.
    template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm = BSMode::BUILD> auto get();
    template <BoostExampleOrTestType EOT, IteratorTargetType iteratorTargetType, BSMode bsm = BSMode::BUILD>
    auto getEndsWith(const char *endsWith = "");

    // TODO
    //  Change ends_with to contains.

    // TODO
    // Currently, not dealing with different kinds of tests and their results and not combining them either.

    // Following 2 functions take a sourceDir and records all the files of the directory at config-time in an array.
    // At build-time, it defines targets using this array.
    template <BoostExampleOrTestType EOT> BoostCppTarget &addDir(string_view sourceDir);

    // This function creates a subdirectory (named innerBuildDirName) within either the Tests or Examples
    // directory, depending on the BoostExampleOrTestType parameter. New targets from sourceDir are built in
    // this subdirectory. This design enables targets with identical names to coexist while having different
    // properties (e.g., macros). Using the alternative function would have caused object-file name collisions
    // in the Tests or Examples directory.
    template <BoostExampleOrTestType EOT>
    BoostCppTarget &addDirEndsWith(string_view sourceDir, string_view innerBuildDirName);

    // Following functions takes an array and defines targets corresponding to that array.
    template <BoostExampleOrTestType EOT>
    BoostCppTarget &add(string_view sourceDir, const string_view *fileNamesArray, uint64_t arraySize);

    // This function is used to avoid object-file name collision just like addDirEndsWith.
    template <BoostExampleOrTestType EOT>
    BoostCppTarget &addEndsWith(string_view innerBuildDirName, string_view sourceDir, const string_view *fileName,
                                uint64_t arraySize);

    template <BoostExampleOrTestType EOT> BoostCppTarget &add(string_view sourceDir, string_view fileName);

    // This function is used to avoid object-file name collision just like addDirEndsWith.
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
        return *exampleOrTest->testTarget.dscTarget;
    }
    else if constexpr (iteratorTargetType == IteratorTargetType::CPP)
    {
        return exampleOrTest->testTarget.dscTarget->getSourceTarget();
    }
    else
    {
        return exampleOrTest->testTarget.dscTarget->getLOAT();
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
        lowerCaseOnWindows(finalEndString.data(), finalEndString.size());
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
                    removeDashCppFromNameSV(exampleOrTest_.testTarget.dscTarget->getSourceTarget().name)
                        .contains(finalEndString))
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
        lowerCaseOnWindows(finalEndString.data(), finalEndString.size());
        for (; exampleOrTest != examplesOrTests.begin().operator->() + examplesOrTests.size(); ++exampleOrTest)
        {
            if (exampleOrTest->targetType == EOT &&
                removeDashCppFromNameSV(exampleOrTest->testTarget.dscTarget->getSourceTarget().name)
                    .contains(finalEndString))
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
    const string configurationNamePlusTargetName =
        removeDashCppFromName(target.mainTarget.getSourceTarget().name) + slashc;

    Node *myBuildDir = nullptr;
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        const string myBuildDirStr = configureNode->filePath + slashc + configurationNamePlusTargetName +
                                     target.getInnerBuildDirExcludingFileName<EOT>();
        myBuildDir = Node::getHalfNodeST(myBuildDirStr);
    }
    const string *pushName =
        new string(target.getInnerBuildDirExcludingFileName<EOT>() + slashc + getNameBeforeLastPeriod(fileName));
    const string name = configurationNamePlusTargetName + *pushName;

    writeUint8(target.configBuffer, static_cast<uint8_t>(EOT));
    writeStringView(target.configBuffer, *pushName);

    target.getTargetFromConfiguration<EOT>(name, myBuildDir, string(sourceDir) + slashc + string(fileName));
}

template <BoostExampleOrTestType EOT>
void BoostCppTarget::Add<EOT, false>::operator()(BoostCppTarget &target, string_view sourceDir, string_view fileName)
{
    const string configurationNamePlusTargetName =
        removeDashCppFromName(target.mainTarget.getSourceTarget().name) + slashc;

    Node *myBuildDir = nullptr;
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        const string myBuildDirStr = configureNode->filePath + slashc + configurationNamePlusTargetName +
                                     target.getInnerBuildDirExcludingFileName<EOT>();
        myBuildDir = Node::getHalfNodeST(myBuildDirStr);
    }

    const string pushName =
        target.getInnerBuildDirExcludingFileName<EOT>() + slashc + getNameBeforeLastPeriod(fileName);
    const string name = configurationNamePlusTargetName + pushName;

    target.getTargetFromConfiguration<EOT>(name, myBuildDir, string(sourceDir) + slashc + string(fileName));
}

template <BoostExampleOrTestType EOT, bool addInConfigCache>
void BoostCppTarget::AddEnds<EOT, addInConfigCache>::operator()(BoostCppTarget &target, string_view innerBuildDirName,
                                                                string_view sourceDir, string_view fileName)
{
    const string configurationNamePlusTargetName =
        removeDashCppFromName(target.mainTarget.getSourceTarget().name) + slashc;

    Node *myBuildDir = nullptr;
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        const string myBuildDirStr = configureNode->filePath + slashc + configurationNamePlusTargetName +
                                     target.getInnerBuildDirExcludingFileName<EOT>();
        myBuildDir = Node::getHalfNodeST(myBuildDirStr);
    }
    const string *pushName = new string(target.getInnerBuildDirExcludingFileName<EOT>(innerBuildDirName) + slashc +
                                        getNameBeforeLastPeriod(fileName));
    const string name = configurationNamePlusTargetName + *pushName;

    writeUint8(target.configBuffer, static_cast<uint8_t>(EOT));
    writeStringView(target.configBuffer, *pushName);

    target.getTargetFromConfiguration<EOT>(name, myBuildDir, string(sourceDir) + slashc + string(fileName));
}

template <BoostExampleOrTestType EOT>
void BoostCppTarget::AddEnds<EOT, false>::operator()(BoostCppTarget &target, string_view innerBuildDirName,
                                                     string_view sourceDir, string_view fileName)
{
    const string configurationNamePlusTargetName =
        removeDashCppFromName(target.mainTarget.getSourceTarget().name) + slashc;

    Node *myBuildDir = nullptr;
    if constexpr (bsMode == BSMode::CONFIGURE)
    {
        const string myBuildDirStr = configureNode->filePath + slashc + configurationNamePlusTargetName +
                                     target.getInnerBuildDirExcludingFileName<EOT>();
        myBuildDir = Node::getHalfNodeST(myBuildDirStr);
    }
    const string pushName =
        target.getInnerBuildDirExcludingFileName<EOT>(innerBuildDirName) + slashc + getNameBeforeLastPeriod(fileName);
    const string name = configurationNamePlusTargetName + pushName;

    target.getTargetFromConfiguration<EOT>(name, myBuildDir, string(sourceDir) + slashc + string(fileName));
}

template <typename T, typename... U> BoostCppTarget &BoostCppTarget::privateTestDeps(T &dep_, U &&...deps_)
{
    if constexpr (std::is_same_v<decltype(dep_), DSC<CppTarget> &>)
    {
        dscTestDepsPrivate.emplace_back(&dep_);
    }
    else if constexpr (std::is_same_v<decltype(dep_), CppTarget &>)
    {
        cppTestDepsPrivate.emplace_back(&dep_);
    }
    else
    {
        static_assert(false && "Unknown Type");
    }

    if constexpr (sizeof...(deps_))
    {
        privateTestDeps(std::forward<U>(deps_)...);
    }
    return *this;
}
template <typename T, typename... U> BoostCppTarget &BoostCppTarget::publicDeps(T &dep_, U &&...deps_)
{
    if constexpr (sizeof...(deps_))
    {
        return deps(DepType::PUBLIC, dep_, std::forward<U>(deps_)...);
    }
    else
    {
        return deps(DepType::PUBLIC, dep_);
    }
}

template <typename T, typename... U> BoostCppTarget &BoostCppTarget::privateDeps(T &dep_, U &&...deps_)
{
    if constexpr (sizeof...(deps_))
    {
        return deps(DepType::PRIVATE, dep_, std::forward<U>(deps_)...);
    }
    else
    {
        return deps(DepType::PRIVATE, dep_);
    }
}

template <typename T, typename... U> BoostCppTarget &BoostCppTarget::interfaceDeps(T &dep_, U &&...deps_)
{
    if constexpr (sizeof...(deps_))
    {
        return deps(DepType::INTERFACE, dep_, std::forward<U>(deps_)...);
    }
    else
    {
        return deps(DepType::INTERFACE, dep_);
    }
}

template <typename T, typename... U> BoostCppTarget &BoostCppTarget::deps(DepType depType, T &dep_, U &&...deps_)
{
    if constexpr (std::is_same_v<decltype(dep_), BoostCppTarget &>)
    {
        mainTarget.deps(depType, dep_.mainTarget);
    }
    else if constexpr (std::is_same_v<DSC<CppTarget> &, decltype(dep_)>)
    {
        mainTarget.deps<CppTarget>(depType, dep_);
    }
    else
    {
        static_assert(false && "No Matching Type");
    }

    if constexpr (sizeof...(deps_))
    {
        return deps(depType, std::forward<U>(deps_)...);
    }
    return *this;
}

template <BoostExampleOrTestType EOT>
void BoostCppTarget::getTargetFromConfiguration(const string_view name, Node *myBuildDir, const string &filePath) const
{
    CppTarget *testOrExmple;
    if constexpr (EOT == BoostExampleOrTestType::COMPILE_TEST)
    {
        CppTarget &t =
            configuration->getCppObjectNoNameAddStdTarget(getExplicitBuilding<EOT>(), myBuildDir, string(name));
        t.privateDeps(mainTarget.getSourceTarget()).moduleFiles(filePath);
        for (CppTarget *dep : cppTestDepsPrivate)
        {
            t.privateDeps(*dep);
        }
        testOrExmple = &t;
    }
    else
    {
        DSC<CppTarget> &t = configuration->getCppExeDSCNoName(getExplicitBuilding<EOT>(), myBuildDir, string(name));
        t.privateDeps(mainTarget).getSourceTarget().moduleFiles(filePath);
        for (DSC<CppTarget> *dep : dscTestDepsPrivate)
        {
            t.privateDeps(*dep);
        }
        testOrExmple = t.getSourceTargetPointer();
    }

    if (bsMode == BSMode::CONFIGURE)
    {
        if (configuration->evaluate(IsCppMod::YES))
        {

            for (const auto &[logicalName, node] : testReqHeaderFiles)
            {
                testOrExmple->addHeaderFile(logicalName, node, false, true, false);
            }

            for (const auto &[logicalName, node] : testReqHeaderUnits)
            {
                testOrExmple->addHeaderUnit(logicalName, node, false, true, false);
            }
        }
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
auto BoostCppTarget::getEndsWith(const char *endsWith)
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
BoostCppTarget &BoostCppTarget::add(string_view sourceDir, const string_view *fileNamesArray, uint64_t arraySize)
{
    for (uint64_t i = 0; i < arraySize; ++i)
    {
        Add<EOT, false>{}(*this, sourceDir, fileNamesArray[i]);
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
